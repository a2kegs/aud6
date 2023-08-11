
This project plays 6-bit audio samples using the Apple II 1-bit speaker.

The inner loop toggles the speaker (if needed) every 65 cycles, for an
effecting sample rate of 15.700KHz.  There will be fairly loud noise at
(and above) this frequency, but most people cannot hear this range well (for
me, I can "hear" something's there, but not really hear it as a tone).

The strategy is a simple PWM, where out of the 65 cycles, if the speaker is
held high for 65 cycles that's the value "65" and if it's held high for 32
and low for 33 it's the value "32", and if it's held high for 0 cycles and
low for 65 cycles it's "0". 

What's new with this approach is it can drive the speaker to any desired
value by using 2 sets of 66 routines: one set to be used when the speaker is
already "high", and one set to be used when the speaker is already "low".
(There's no way to tell at the start if the speaker is high or low, but it
doesn't matter, driving an inverted audio signal sounds the same, but it's
important for the code to remember the speaker toggle state).

See the Details.txt file for details on how it is supposed to work.

-----

I'm trying something new: I'm releasing a project when it DOESN'T WORK.
I don't know why it doesn't work.  When I attempt to play the provided
sample (4 seconds from Wang Chung's "To Live and Die in LA", which is
absolutely fair use), it sounds terribly distorted.  This is in KEGS--which
is not trying to model any speaker shenanigans.  It should sound right--and
it doesn't.  I suspect a bug, but there may be a deeper problem.

The source .WAV file is "live.and.die.la.wav", and I converted it to
6-bit 15.7KHz n modwave.wav (but still keeping the sample rate at 44.1KHz,
see Sample_rate.txt for details).  It sounds right, but with some "noise",
which makes sense.  But when played on KEGS (and real IIgs hardware), it
sounds like distorted guitar sounds--like the sound is being clipped.

