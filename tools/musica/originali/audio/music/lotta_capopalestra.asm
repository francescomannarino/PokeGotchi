; Lotta contro un capopalestra
; Brano originale scritto per PokeGotchi, coperto dalla licenza del progetto (MIT).
; Sol minore, tempo 104: introduzione di 4 battute, giro A B A B di 32 battute.

Music_LottaCapopalestra_Ch1::
	tempo 104
	volume 7, 7
	duty_cycle 3
	vibrato 12, 2, 4
	toggle_perfect_pitch
	note_type 12, 10, 1
; introduzione: tremolo di sedicesimi su Sol minore, Mib, Fa e Re
	octave 4
	note G_, 1
	note D_, 1
	note A#, 1
	note D_, 1
	note G_, 1
	note D_, 1
	note A#, 1
	note D_, 1
	note G_, 1
	note D_, 1
	note A#, 1
	note D_, 1
	note G_, 1
	note D_, 1
	note A#, 1
	note D_, 1
	note G_, 1
	note D#, 1
	note A#, 1
	note D#, 1
	note G_, 1
	note D#, 1
	note A#, 1
	note D#, 1
	note G_, 1
	note D#, 1
	note A#, 1
	note D#, 1
	note G_, 1
	note D#, 1
	note A#, 1
	note D#, 1
	note A_, 1
	note F_, 1
	octave 5
	note C_, 1
	octave 4
	note F_, 1
	note A_, 1
	note F_, 1
	octave 5
	note C_, 1
	octave 4
	note F_, 1
	note A_, 1
	note F_, 1
	octave 5
	note C_, 1
	octave 4
	note F_, 1
	note A_, 1
	note F_, 1
	octave 5
	note C_, 1
	octave 4
	note F_, 1
	note_type 12, 12, 4
	note F#, 4
	note A_, 4
	octave 5
	note D_, 8

.mainloop:
	sound_call .a
	sound_call .b
	sound_call .a
	sound_call .b
	sound_loop 0, .mainloop

; A: Gm Gm Eb F Gm Gm Cm D
.a:
	note_type 12, 12, 5
	octave 4
	note G_, 3
	note A#, 3
	octave 5
	note D_, 2
	note C_, 4
	octave 4
	note A#, 4
	note A_, 3
	note G_, 3
	note F_, 2
	note G_, 8
	note D#, 3
	note G_, 3
	note A#, 2
	octave 5
	note D#, 4
	note D_, 4
	note C_, 6
	octave 4
	note A_, 2
	note F_, 8
	note G_, 3
	note A#, 3
	octave 5
	note D_, 2
	note G_, 4
	note F_, 4
	note D_, 3
	note C_, 3
	octave 4
	note A#, 2
	note A_, 8
	note G_, 4
	octave 5
	note C_, 4
	note D#, 4
	note D_, 2
	note C_, 2
	note D_, 8
	octave 4
	note F#, 4
	note A_, 4
	sound_ret

; B: Eb Bb Cm Gm Eb F D D
.b:
	note_type 12, 11, 7
	octave 4
	note A#, 6
	note G_, 2
	note D#, 8
	note F_, 4
	note A#, 4
	octave 5
	note D_, 8
	note D#, 6
	note D_, 2
	note C_, 4
	octave 4
	note G_, 4
	note A#, 12
	note A_, 4
	note G_, 6
	note A#, 2
	octave 5
	note D#, 4
	note G_, 4
	note F_, 6
	note D#, 2
	note D_, 4
	note C_, 4
	note D_, 4
	octave 4
	note A_, 4
	note F#, 4
	note A_, 4
	octave 5
	note C_, 4
	octave 4
	note A_, 4
	note F#, 4
	note D_, 4
	sound_ret

Music_LottaCapopalestra_Ch2::
	duty_cycle 2
	vibrato 10, 1, 5
	note_type 12, 9, 7
	octave 3
	note A#, 16
	note A#, 16
	octave 4
	note C_, 16
	octave 3
	note A_, 8
	octave 4
	note D_, 8

.mainloop:
	sound_call .a
	sound_call .b
	sound_call .a
	sound_call .b
	sound_loop 0, .mainloop

.a:
	note_type 12, 9, 2
	octave 3
	note G_, 3
	note A#, 3
	note G_, 2
	note G_, 3
	note A#, 3
	note G_, 2
	note G_, 3
	note A#, 3
	note G_, 2
	note G_, 3
	note A#, 3
	note G_, 2
	note G_, 3
	note A#, 3
	note G_, 2
	note G_, 3
	note A#, 3
	note G_, 2
	note F_, 3
	note A_, 3
	note F_, 2
	note F_, 3
	note A_, 3
	note F_, 2
	note G_, 3
	note A#, 3
	note G_, 2
	note G_, 3
	note A#, 3
	note G_, 2
	note G_, 3
	note A#, 3
	note G_, 2
	note G_, 3
	note A#, 3
	note G_, 2
	note D#, 3
	note G_, 3
	note D#, 2
	note D#, 3
	note G_, 3
	note D#, 2
	note F#, 3
	note A_, 3
	note F#, 2
	note F#, 3
	note A_, 3
	note F#, 2
	sound_ret

.b:
	note_type 12, 9, 3
	octave 3
	note D#, 2
	note A#, 2
	note G_, 2
	note A#, 2
	note D#, 2
	note A#, 2
	note G_, 2
	note A#, 2
	note D_, 2
	note A#, 2
	note F_, 2
	note A#, 2
	note D_, 2
	note A#, 2
	note F_, 2
	note A#, 2
	note C_, 2
	note G_, 2
	note D#, 2
	note G_, 2
	note C_, 2
	note G_, 2
	note D#, 2
	note G_, 2
	note D_, 2
	note A#, 2
	note G_, 2
	note A#, 2
	note D_, 2
	note A#, 2
	note G_, 2
	note A#, 2
	note D#, 2
	note A#, 2
	note G_, 2
	note A#, 2
	note D#, 2
	note A#, 2
	note G_, 2
	note A#, 2
	note C_, 2
	note A_, 2
	note F_, 2
	note A_, 2
	note C_, 2
	note A_, 2
	note F_, 2
	note A_, 2
	note D_, 2
	note A_, 2
	note F#, 2
	note A_, 2
	note D_, 2
	note A_, 2
	note F#, 2
	note A_, 2
	note D_, 2
	note A_, 2
	note F#, 2
	note A_, 2
	note D_, 2
	note A_, 2
	note F#, 2
	note A_, 2
	sound_ret

Music_LottaCapopalestra_Ch3::
	note_type 12, 1, 2
	octave 2
	note G_, 16
	octave 3
	note D#, 16
	note F_, 16
	note D_, 8
	octave 4
	note D_, 8

.mainloop:
	sound_call .a
	sound_call .b
	sound_call .a
	sound_call .b
	sound_loop 0, .mainloop

.a:
	octave 2
	note G_, 3
	note G_, 3
	octave 3
	note G_, 2
	octave 2
	note G_, 3
	note G_, 3
	octave 3
	note G_, 2
	octave 2
	note G_, 3
	note G_, 3
	octave 3
	note G_, 2
	octave 2
	note G_, 3
	note G_, 3
	octave 3
	note G_, 2
	note D#, 3
	note D#, 3
	octave 4
	note D#, 2
	octave 3
	note D#, 3
	note D#, 3
	octave 4
	note D#, 2
	octave 2
	note F_, 3
	note F_, 3
	octave 3
	note F_, 2
	octave 2
	note F_, 3
	note F_, 3
	octave 3
	note F_, 2
	octave 2
	note G_, 3
	note G_, 3
	octave 3
	note G_, 2
	octave 2
	note G_, 3
	note G_, 3
	octave 3
	note G_, 2
	octave 2
	note G_, 3
	note G_, 3
	octave 3
	note G_, 2
	octave 2
	note G_, 3
	note G_, 3
	octave 3
	note G_, 2
	note C_, 3
	note C_, 3
	octave 4
	note C_, 2
	octave 3
	note C_, 3
	note C_, 3
	octave 4
	note C_, 2
	octave 3
	note D_, 3
	note D_, 3
	octave 4
	note D_, 2
	octave 3
	note D_, 3
	note D_, 3
	octave 4
	note D_, 2
	sound_ret

.b:
	octave 3
	note D#, 2
	octave 4
	note D#, 2
	octave 3
	note D#, 2
	octave 4
	note D#, 2
	octave 3
	note D#, 2
	octave 4
	note D#, 2
	octave 3
	note D#, 2
	octave 4
	note D#, 2
	octave 2
	note A#, 2
	octave 3
	note A#, 2
	octave 2
	note A#, 2
	octave 3
	note A#, 2
	octave 2
	note A#, 2
	octave 3
	note A#, 2
	octave 2
	note A#, 2
	octave 3
	note A#, 2
	note C_, 2
	octave 4
	note C_, 2
	octave 3
	note C_, 2
	octave 4
	note C_, 2
	octave 3
	note C_, 2
	octave 4
	note C_, 2
	octave 3
	note C_, 2
	octave 4
	note C_, 2
	octave 2
	note G_, 2
	octave 3
	note G_, 2
	octave 2
	note G_, 2
	octave 3
	note G_, 2
	octave 2
	note G_, 2
	octave 3
	note G_, 2
	octave 2
	note G_, 2
	octave 3
	note G_, 2
	note D#, 2
	octave 4
	note D#, 2
	octave 3
	note D#, 2
	octave 4
	note D#, 2
	octave 3
	note D#, 2
	octave 4
	note D#, 2
	octave 3
	note D#, 2
	octave 4
	note D#, 2
	octave 2
	note F_, 2
	octave 3
	note F_, 2
	octave 2
	note F_, 2
	octave 3
	note F_, 2
	octave 2
	note F_, 2
	octave 3
	note F_, 2
	octave 2
	note F_, 2
	octave 3
	note F_, 2
	note D_, 2
	octave 4
	note D_, 2
	octave 3
	note D_, 2
	octave 4
	note D_, 2
	octave 3
	note D_, 2
	octave 4
	note D_, 2
	octave 3
	note D_, 2
	octave 4
	note D_, 2
	octave 3
	note D_, 2
	octave 4
	note D_, 2
	octave 3
	note D_, 2
	octave 4
	note D_, 2
	octave 3
	note D_, 2
	octave 4
	note D_, 2
	octave 3
	note D_, 2
	octave 4
	note D_, 2
	sound_ret
