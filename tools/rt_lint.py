#!/usr/bin/env python3
"""Static audio-thread lint.

Complements the runtime allocation detector (RtGuard): it fails the build when code that runs on the
audio thread mentions a construct that allocates, locks, blocks, throws, logs or does I/O.

What counts as "audio-thread code" is declared below:
  * RT_FILES      - every line of these files is audio-thread code
  * RT_FUNCTIONS  - only the bodies of the named functions in these files

A line can opt out with a trailing   // rt-lint: allow <reason>   (reviewers read the reason).

Usage:  rt_lint.py <repo root>        lint the repository
        rt_lint.py --self-test        prove that the rules actually fire (used by ctest)
"""
import os
import re
import sys

# --------------------------------------------------------------------------------------------------
# Scope
# --------------------------------------------------------------------------------------------------
RT_FILES = [
    "native/core/include/autodj/core/SpscQueue.h",
    "native/core/include/autodj/core/TripleBuffer.h",
    "native/core/include/autodj/core/Transport.h",
    "native/core/include/autodj/core/Dsp.h",
    "native/core/include/autodj/core/Denormals.h",
    "native/core/include/autodj/core/Clock.h",
    "native/core/include/autodj/core/Command.h",
    "native/core/include/autodj/core/Status.h",
    "native/core/include/autodj/core/Node.h",
    "native/core/include/autodj/core/SourceTable.h",
    "native/granular/include/autodj/granular/GrainPool.h",
    "native/granular/include/autodj/granular/GrainScheduler.h",
    "native/granular/include/autodj/granular/AdaptiveQuality.h",
    "native/granular/include/autodj/granular/GrainKernels.h",
    "native/granular/src/GrainKernels.cpp",
    "native/engine/src/ToneNode.cpp",
    "native/dj/include/autodj/dj/DjDsp.h",
]

RT_FUNCTIONS = {
    "native/granular/src/GranularNode.cpp": [
        "reset", "smoothedId", "setParams", "handleCommand", "clearGrains", "stealVoiceGrains", "process",
        "controlVoice", "spawnGrain", "renderGrains", "mixVoices", "publishViz", "stats", "stepCell",
    ],
    "native/engine/src/Engine.cpp": [
        "drainCommands", "applyCommand", "process", "processChunk", "publishStatus",
    ],
    "native/core/include/autodj/core/Param.h": ["get", "getInt", "getBool"],
    "native/fx/src/ChaosFx.cpp": ["map", "reset", "clearReverb", "clearFlanger", "process"],
    "native/dj/src/Deck.cpp": ["setTrack", "play", "pause", "seek", "heardFrames", "fillInput", "startStream", "process"],
    "native/dj/src/DjNode.cpp": [
        "setParams", "release", "clearManualDecks", "finishPlan", "stopPlan", "handleCommand", "trimGain", "crossfaderGains",
        "tone", "processLive", "startItem", "fireDue", "renderPlanBlock", "processPlan", "process", "fillStatus",
    ],
}

# --------------------------------------------------------------------------------------------------
# Rules: (regex on comment/string-stripped code, explanation)
# --------------------------------------------------------------------------------------------------
RULES = [
    (r"\bnew\b", "heap allocation (new)"),
    (r"\bdelete\b", "heap free (delete)"),
    (r"(?<![\w.>])(?:std::|::)?(malloc|calloc|realloc|free|aligned_alloc|posix_memalign|strdup)\s*\(", "C heap call"),
    (r"\b(make_unique|make_shared|allocate_shared)\b", "heap allocation (make_*)"),
    (r"\bstd::(vector|string|map|unordered_map|set|unordered_set|list|deque|queue|stringstream|ostringstream)\b",
     "allocating standard container / string"),
    (r"\.(push_back|emplace_back|emplace|resize|reserve|shrink_to_fit)\s*\(", "container growth"),
    (r"\bstd::(function|any|shared_ptr|weak_ptr)\b", "allocating or atomic-refcounting wrapper"),
    (r"\bstd::(to_string|stoi|stof|stod)\b", "string conversion (allocates / may throw)"),
    (r"\bstd::(mutex|recursive_mutex|shared_mutex|timed_mutex|lock_guard|unique_lock|scoped_lock|"
     r"condition_variable|counting_semaphore|binary_semaphore|barrier|latch)\b", "blocking synchronisation"),
    (r"\bpthread_(mutex|cond|rwlock|join|create)\w*\b", "blocking pthread call"),
    (r"\bstd::(thread|jthread|async|promise|future)\b", "thread creation / blocking wait"),
    (r"\bstd::this_thread::(sleep_for|sleep_until|yield)\b", "sleeping on the audio thread"),
    (r"\b(usleep|nanosleep|sleep)\s*\(", "sleeping on the audio thread"),
    (r"\bthrow\b", "exception"),
    (r"\btry\s*\{", "exception handling"),
    (r"\bcatch\s*\(", "exception handling"),
    (r"\b(dynamic_cast|typeid)\b", "RTTI"),
    (r"\b(printf|fprintf|sprintf|snprintf|puts|fputs|perror)\s*\(", "stdio"),
    (r"\bstd::(cout|cerr|clog|cin|endl)\b", "iostream"),
    (r"(?<![\w.>])(::)?(fopen|fread|fwrite|fclose|mmap|munmap|fsync)\s*\(|(?<![\w.>])::(open|read|write|close)\s*\(",
     "file / syscall I/O"),
    (r"\bstd::(ifstream|ofstream|fstream|filesystem)\b", "file I/O"),
    (r"\b__android_log_\w+\b", "logcat (takes locks, does I/O)"),
    (r"\bALOG[VDIWEF]\b", "logcat"),
    (r"\b(JNIEnv|JavaVM|jobject|jclass|jmethodID)\b", "JNI"),
    (r"\b(system|popen|exit|abort)\s*\(", "process-level call"),
]
COMPILED = [(re.compile(p), why) for p, why in RULES]
ALLOW = re.compile(r"//\s*rt-lint:\s*allow\b")


# --------------------------------------------------------------------------------------------------
# Source handling
# --------------------------------------------------------------------------------------------------
def strip_code(text):
    """Blanks comments, string and char literals but keeps line structure and the allow-markers."""
    out = []
    i, n = 0, len(text)
    allow_lines = set()
    line = 1
    while i < n:
        c = text[i]
        two = text[i:i + 2]
        if two == "//":
            j = text.find("\n", i)
            j = n if j < 0 else j
            if ALLOW.search(text[i:j]):
                allow_lines.add(line)
            out.append(" " * (j - i))
            i = j
        elif two == "/*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            chunk = text[i:j]
            line += chunk.count("\n")
            out.append("".join(ch if ch == "\n" else " " for ch in chunk))
            i = j
        elif c == '"' or c == "'":
            # raw strings are not used in audio code; plain literals only
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(c + " " * (j - i - 2) + c if j - i >= 2 else c)
            i = j
        else:
            if c == "\n":
                line += 1
            out.append(c)
            i += 1
    return "".join(out), allow_lines


def function_bodies(code, names):
    """Yields (name, first_line, last_line) for definitions of the given function names."""
    result = []
    for name in names:
        # definition: optional qualifier, name, parameter list, optional const/noexcept/override, '{'
        pat = re.compile(r"(?<![\w.>])(?:\w+::)*" + re.escape(name) + r"\s*\([^;{}]*\)\s*(?:const)?\s*(?:noexcept)?\s*"
                         r"(?:const)?\s*(?:->\s*[\w:<>\s&*]+)?\s*\{", re.S)
        for m in pat.finditer(code):
            depth, i = 0, m.end() - 1
            while i < len(code):
                if code[i] == "{":
                    depth += 1
                elif code[i] == "}":
                    depth -= 1
                    if depth == 0:
                        break
                i += 1
            first = code.count("\n", 0, m.start()) + 1
            last = code.count("\n", 0, i) + 1
            result.append((name, first, last))
    return result


def lint_text(text, only_functions=None, label="<text>"):
    code, allow = strip_code(text)
    lines = code.split("\n")
    if only_functions is None:
        scope = set(range(1, len(lines) + 1))
        found = None
    else:
        scope = set()
        found = set()
        for name, a, b in function_bodies(code, only_functions):
            scope.update(range(a, b + 1))
            found.add(name)
    problems = []
    for ln in sorted(scope):
        if ln in allow:
            continue
        s = re.sub(r"=\s*delete\b", "=", lines[ln - 1])  # deleted special members are fine
        for rx, why in COMPILED:
            if rx.search(s):
                problems.append((label, ln, why, s.strip()))
    missing = [] if found is None else [f for f in only_functions if f not in found]
    return problems, missing


def lint_repo(root):
    problems, config_errors = [], []
    for rel in RT_FILES:
        path = os.path.join(root, rel)
        if not os.path.exists(path):
            config_errors.append("configured audio-thread file does not exist: " + rel)
            continue
        p, _ = lint_text(open(path, encoding="utf-8").read(), None, rel)
        problems += p
    for rel, funcs in RT_FUNCTIONS.items():
        path = os.path.join(root, rel)
        if not os.path.exists(path):
            config_errors.append("configured audio-thread file does not exist: " + rel)
            continue
        p, missing = lint_text(open(path, encoding="utf-8").read(), funcs, rel)
        problems += p
        for f in missing:
            config_errors.append("function '%s' not found in %s (renamed? update tools/rt_lint.py)" % (f, rel))
    return problems, config_errors


# --------------------------------------------------------------------------------------------------
# Self test: every rule must be provoked at least once, allowed lines and comments must stay quiet.
# --------------------------------------------------------------------------------------------------
SELF_TEST_BAD = [
    "int* p = new int[4];", "free(p);", "auto u = std::make_unique<int>(3);", "std::vector<float> v;",
    "v.push_back(1);", "std::mutex m;", "std::lock_guard<std::mutex> l(m);", "throw 1;", "try { f(); }",
    "printf(\"x\");", "std::cout << 1;", "std::function<void()> f;", "std::shared_ptr<int> s;",
    "__android_log_print(1, \"t\", \"x\");", "JNIEnv* env;", "std::this_thread::sleep_for(x);",
    "fopen(\"a\", \"r\");", "std::string s;", "std::to_string(3);",
]
SELF_TEST_GOOD = [
    "float x = a * b;", "// new int[4] in a comment", "const char* s = \"new delete malloc\";",
    "Foo(const Foo&) = delete;", "std::vector<float> v;  // rt-lint: allow control-side only",
    "alignas(64) float buf[128];", "int renew = 1;", "auto n = std::atomic<int>{};",
]


def self_test():
    ok = True
    for src in SELF_TEST_BAD:
        p, _ = lint_text(src + "\n")
        if not p:
            print("self-test FAILED: rule did not fire for: " + src)
            ok = False
    for src in SELF_TEST_GOOD:
        p, _ = lint_text(src + "\n")
        if p:
            print("self-test FAILED: false positive for: %s (%s)" % (src, p[0][2]))
            ok = False
    body = "void A::f() noexcept {\n  int a = 1;\n}\nvoid A::g() {\n  auto* p = new int;\n}\n"
    p, missing = lint_text(body, ["f"])
    if p or missing:
        print("self-test FAILED: function scoping flagged code outside the function")
        ok = False
    p, _ = lint_text(body, ["g"])
    if not p:
        print("self-test FAILED: function scoping missed code inside the function")
        ok = False
    if ok:
        print("rt_lint self-test OK (%d rules, %d bad and %d good samples)" % (len(RULES), len(SELF_TEST_BAD), len(SELF_TEST_GOOD)))
    return 0 if ok else 1


def main(argv):
    if len(argv) >= 2 and argv[1] == "--self-test":
        return self_test()
    root = argv[1] if len(argv) >= 2 else os.getcwd()
    problems, config_errors = lint_repo(root)
    for e in config_errors:
        print("rt_lint: config error: " + e)
    for label, ln, why, src in problems:
        print("%s:%d: audio-thread code must not use: %s\n    %s" % (label, ln, why, src))
    if problems or config_errors:
        print("rt_lint: %d violation(s), %d config error(s)" % (len(problems), len(config_errors)))
        return 1
    print("rt_lint: OK (%d files, %d function groups)" % (len(RT_FILES), len(RT_FUNCTIONS)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
