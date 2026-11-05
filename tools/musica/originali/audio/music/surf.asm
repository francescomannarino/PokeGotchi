; Surf
; Brano originale scritto per PokeGotchi, coperto dalla licenza del progetto (MIT).
; La maggiore, tempo 116: introduzione di 2 battute, giro di 16 battute con il basso a ritmo 3+3+2.

Music_Surf_Ch1::
	tempo 116
	volume 7, 7
	duty_cycle 2
	vibrato 6, 1, 5
	note_type 12, 11, 2
; introduzione: A E
	octave 4
	note E_, 2
	rest 2
	note C#, 2
	note E_, 2
	rest 2
	note A_, 2
	rest 2
	note E_, 2
	note D_, 2
	rest 2
	octave 3
	note B_, 2
	octave 4
	note D_, 2
	rest 2
	note G#, 2
	rest 2
	note E_, 2

.mainloop:
	note_type 12, 11, 3
; A: A D E A F#m D E E
	octave 4
	note C#, 3
	note E_, 3
	note A_, 2
	rest 2
	note A_, 2
	note G#, 2
	note A_, 2
	note F#, 3
	note A_, 3
	note F#, 2
	note E_, 4
	note D_, 4
	note E_, 3
	note G#, 3
	note B_, 2
	rest 2
	note B_, 2
	note A_, 2
	note G#, 2
	note A_, 8
	note E_, 4
	note C#, 4
	note F#, 3
	note A_, 3
	octave 5
	note C#, 2
	rest 2
	note C#, 2
	octave 4
	note B_, 2
	note A_, 2
	octave 5
	note D_, 6
	octave 4
	note B_, 2
	note A_, 4
	note F#, 4
	note E_, 3
	note F#, 3
	note G#, 2
	note B_, 4
	note G#, 4
	note E_, 12
	rest 4
	note_type 12, 11, 2
; B: D E C#m F#m D E A A
	note A_, 2
	rest 2
	note A_, 2
	note B_, 2
	note A_, 4
	note F#, 4
	note G#, 2
	rest 2
	note G#, 2
	note A_, 2
	note B_, 8
	octave 5
	note C#, 2
	rest 2
	note C#, 2
	note E_, 2
	octave 4
	note B_, 4
	note G#, 4
	note A_, 6
	note G#, 2
	note F#, 8
	note F#, 2
	note A_, 2
	octave 5
	note D_, 2
	rest 2
	note C#, 2
	octave 4
	note B_, 2
	note A_, 4
	note G#, 2
	note B_, 2
	octave 5
	note E_, 2
	rest 2
	note D_, 2
	note C#, 2
	octave 4
	note B_, 4
	note A_, 3
	octave 5
	note C#, 3
	note E_, 2
	note C#, 4
	octave 4
	note A_, 4
	note A_, 8
	rest 4
	note E_, 4
	sound_loop 0, .mainloop

Music_Surf_Ch2::
	duty_cycle 1
	note_type 12, 8, 1
	rest 2
	octave 3
	note E_, 2
	rest 2
	note A_, 2
	rest 2
	note E_, 2
	rest 2
	note A_, 2
	rest 2
	note G#, 2
	rest 2
	note B_, 2
	rest 2
	note G#, 2
	rest 2
	note B_, 2

.mainloop:
	rest 2
	octave 3
	note E_, 2
	rest 2
	note A_, 2
	rest 2
	note E_, 2
	rest 2
	note A_, 2
	rest 2
	note F#, 2
	rest 2
	note A_, 2
	rest 2
	note F#, 2
	rest 2
	note A_, 2
	rest 2
	note G#, 2
	rest 2
	note B_, 2
	rest 2
	note G#, 2
	rest 2
	note B_, 2
	rest 2
	note E_, 2
	rest 2
	note A_, 2
	rest 2
	note E_, 2
	rest 2
	note A_, 2
	rest 2
	note F#, 2
	rest 2
	note A_, 2
	rest 2
	note F#, 2
	rest 2
	note A_, 2
	rest 2
	note F#, 2
	rest 2
	note A_, 2
	rest 2
	note F#, 2
	rest 2
	note A_, 2
	rest 2
	note G#, 2
	rest 2
	note B_, 2
	rest 2
	note G#, 2
	rest 2
	note B_, 2
	rest 2
	note G#, 2
	rest 2
	note B_, 2
	rest 2
	note G#, 2
	rest 2
	note B_, 2
	rest 2
	note F#, 2
	rest 2
	note A_, 2
	rest 2
	note F#, 2
	rest 2
	note A_, 2
	rest 2
	note G#, 2
	rest 2
	note B_, 2
	rest 2
	note G#, 2
	rest 2
	note B_, 2
	rest 2
	note E_, 2
	rest 2
	note G#, 2
	rest 2
	note E_, 2
	rest 2
	note G#, 2
	rest 2
	note F#, 2
	rest 2
	note A_, 2
	rest 2
	note F#, 2
	rest 2
	note A_, 2
	rest 2
	note F#, 2
	rest 2
	note A_, 2
	rest 2
	note F#, 2
	rest 2
	note A_, 2
	rest 2
	note G#, 2
	rest 2
	note B_, 2
	rest 2
	note G#, 2
	rest 2
	note B_, 2
	rest 2
	note E_, 2
	rest 2
	note A_, 2
	rest 2
	note E_, 2
	rest 2
	note A_, 2
	rest 2
	note E_, 2
	rest 2
	note A_, 2
	rest 2
	note E_, 2
	rest 2
	note A_, 2
	sound_loop 0, .mainloop

Music_Surf_Ch3::
	note_type 12, 1, 3
	octave 2
	note A_, 3
	note A_, 3
	octave 3
	note E_, 2
	note A_, 3
	octave 2
	note A_, 3
	octave 3
	note E_, 2
	note E_, 3
	note E_, 3
	note B_, 2
	octave 4
	note E_, 3
	octave 3
	note E_, 3
	note B_, 2

.mainloop:
	octave 2
	note A_, 3
	note A_, 3
	octave 3
	note E_, 2
	note A_, 3
	octave 2
	note A_, 3
	octave 3
	note E_, 2
	note D_, 3
	note D_, 3
	note A_, 2
	octave 4
	note D_, 3
	octave 3
	note D_, 3
	note A_, 2
	note E_, 3
	note E_, 3
	note B_, 2
	octave 4
	note E_, 3
	octave 3
	note E_, 3
	note B_, 2
	octave 2
	note A_, 3
	note A_, 3
	octave 3
	note E_, 2
	note A_, 3
	octave 2
	note A_, 3
	octave 3
	note E_, 2
	octave 2
	note F#, 3
	note F#, 3
	octave 3
	note C#, 2
	note F#, 3
	octave 2
	note F#, 3
	octave 3
	note C#, 2
	note D_, 3
	note D_, 3
	note A_, 2
	octave 4
	note D_, 3
	octave 3
	note D_, 3
	note A_, 2
	note E_, 3
	note E_, 3
	note B_, 2
	octave 4
	note E_, 3
	octave 3
	note E_, 3
	note B_, 2
	note E_, 3
	note E_, 3
	note B_, 2
	octave 4
	note E_, 3
	octave 3
	note E_, 3
	note B_, 2
	note D_, 3
	note D_, 3
	note A_, 2
	octave 4
	note D_, 3
	octave 3
	note D_, 3
	note A_, 2
	note E_, 3
	note E_, 3
	note B_, 2
	octave 4
	note E_, 3
	octave 3
	note E_, 3
	note B_, 2
	note C#, 3
	note C#, 3
	note G#, 2
	octave 4
	note C#, 3
	octave 3
	note C#, 3
	note G#, 2
	octave 2
	note F#, 3
	note F#, 3
	octave 3
	note C#, 2
	note F#, 3
	octave 2
	note F#, 3
	octave 3
	note C#, 2
	note D_, 3
	note D_, 3
	note A_, 2
	octave 4
	note D_, 3
	octave 3
	note D_, 3
	note A_, 2
	note E_, 3
	note E_, 3
	note B_, 2
	octave 4
	note E_, 3
	octave 3
	note E_, 3
	note B_, 2
	octave 2
	note A_, 3
	note A_, 3
	octave 3
	note E_, 2
	note A_, 3
	octave 2
	note A_, 3
	octave 3
	note E_, 2
	octave 2
	note A_, 3
	note A_, 3
	octave 3
	note E_, 2
	note A_, 3
	octave 2
	note A_, 3
	octave 3
	note E_, 2
	sound_loop 0, .mainloop
