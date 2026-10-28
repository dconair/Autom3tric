Decoder fixtures: 2 s, 44.1 kHz, 440 Hz sine at -6 dBFS (amplitude 0.5) on the left channel and half of it on the right.
Generated with ffmpeg (libmp3lame 96k, flac, libvorbis q3, aac 96k):

    ffmpeg -f lavfi -i sine=frequency=440:duration=2:sample_rate=44100 -af "volume=4,pan=stereo|c0=c0|c1=0.5*c0" <codec options>  (flac: -sample_fmt s16) tone.<ext>
