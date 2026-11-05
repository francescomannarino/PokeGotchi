; Lotta contro un selvatico
; Brano originale scritto per PokeGotchi, coperto dalla licenza del progetto (MIT).
; Re minore, tempo 104: introduzione di 4 battute, giro A B A di 24 battute.

Music_LottaSelvatico_Ch1::
	tempo 104
	volume 7, 7
	duty_cycle 3
	vibrato 10, 2, 4
	toggle_perfect_pitch
	note_type 12, 12, 2
; introduzione: colpi ribattuti e una scala spezzata che sale
	octave 4
	note D_, 2
	rest 2
	note D_, 2
	rest 2
	note D_, 2
	rest 2
	note F_, 2
	note E_, 2
	note D_, 2
	rest 2
	note D_, 2
	rest 2
	note D_, 2
	rest 2
	note A_, 2
	note G#, 2
	octave 3
	note D_, 1
	note F_, 1
	note A_, 1
	octave 4
	note D_, 1
	octave 3
	note F_, 1
	note A_, 1
	octave 4
	note D_, 1
	note F_, 1
	octave 3
	note A_, 1
	octave 4
	note D_, 1
	note F_, 1
	note A_, 1
	note D_, 1
	note F_, 1
	note A_, 1
	octave 5
	note D_, 1
	octave 4
	note E_, 4
	note C#, 4
	octave 3
	note A_, 8

.mainloop:
	sound_call .tema
	note_type 12, 11, 4
; B: Bb C Am Dm Gm Bb A A
	octave 4
	note F_, 4
	note D_, 4
	note F_, 4
	note A#, 4
	octave 5
	note C_, 6
	octave 4
	note A#, 2
	note A_, 4
	note G_, 4
	note A_, 4
	note E_, 4
	note A_, 4
	octave 5
	note C_, 4
	note D_, 12
	note C_, 2
	octave 4
	note A_, 2
	note A#, 6
	note A_, 2
	note G_, 4
	note D_, 4
	note F_, 6
	note G_, 2
	note A_, 4
	note A#, 4
	octave 5
	note C#, 8
	note E_, 8
	octave 4
	note A_, 4
	note G_, 4
	note F_, 4
	note E_, 4
	sound_call .tema
	sound_loop 0, .mainloop

; A: Dm Dm Bb C Dm Dm Gm A
.tema:
	note_type 12, 11, 5
	octave 4
	note D_, 6
	note E_, 2
	note F_, 4
	note A_, 4
	octave 5
	note C_, 4
	octave 4
	note A_, 2
	note G_, 2
	note F_, 4
	note E_, 4
	note F_, 6
	note G_, 2
	note A#, 4
	note F_, 4
	note G_, 6
	note A_, 2
	note G_, 4
	note E_, 4
	note D_, 6
	note E_, 2
	note F_, 4
	note A_, 4
	octave 5
	note D_, 4
	note C_, 2
	octave 4
	note A_, 2
	note F_, 8
	note G_, 4
	note A#, 4
	octave 5
	note D_, 4
	note C_, 2
	octave 4
	note A#, 2
	note A_, 10
	note G_, 2
	note F_, 2
	note E_, 2
	sound_ret

Music_LottaSelvatico_Ch2::
	duty_cycle 1
	vibrato 8, 1, 5
	note_type 12, 10, 2
	octave 3
	note A_, 2
	rest 2
	note A_, 2
	rest 2
	note A_, 2
	rest 2
	octave 4
	note C_, 2
	octave 3
	note A#, 2
	note A_, 2
	rest 2
	note A_, 2
	rest 2
	note A_, 2
	rest 2
	octave 4
	note E_, 2
	note D_, 2
	octave 3
	note F_, 4
	note A_, 4
	octave 4
	note D_, 4
	note F_, 4
	note C#, 4
	octave 3
	note A_, 4
	note E_, 8

.mainloop:
	sound_call .tema
	note_type 12, 8, 1
	rest 2
	octave 3
	note F_, 2
	rest 2
	note A#, 2
	rest 2
	note F_, 2
	rest 2
	note A#, 2
	rest 2
	note G_, 2
	rest 2
	octave 4
	note C_, 2
	rest 2
	octave 3
	note G_, 2
	rest 2
	octave 4
	note C_, 2
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
	note F_, 2
	rest 2
	note A_, 2
	rest 2
	note F_, 2
	rest 2
	note A_, 2
	rest 2
	note G_, 2
	rest 2
	note A#, 2
	rest 2
	note G_, 2
	rest 2
	note A#, 2
	rest 2
	note F_, 2
	rest 2
	note A#, 2
	rest 2
	note F_, 2
	rest 2
	note A#, 2
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
	sound_call .tema
	sound_loop 0, .mainloop

.tema:
	note_type 12, 9, 2
	octave 3
	note D_, 2
	note F_, 2
	note A_, 2
	note F_, 2
	note D_, 2
	note F_, 2
	note A_, 2
	note F_, 2
	note D_, 2
	note F_, 2
	note A_, 2
	note F_, 2
	note D_, 2
	note F_, 2
	note A_, 2
	note F_, 2
	note D_, 2
	note F_, 2
	note A#, 2
	note F_, 2
	note D_, 2
	note F_, 2
	note A#, 2
	note F_, 2
	note E_, 2
	note G_, 2
	octave 4
	note C_, 2
	octave 3
	note G_, 2
	note E_, 2
	note G_, 2
	octave 4
	note C_, 2
	octave 3
	note G_, 2
	note D_, 2
	note F_, 2
	note A_, 2
	note F_, 2
	note D_, 2
	note F_, 2
	note A_, 2
	note F_, 2
	note D_, 2
	note F_, 2
	note A_, 2
	note F_, 2
	note D_, 2
	note F_, 2
	note A_, 2
	note F_, 2
	note D_, 2
	note G_, 2
	note A#, 2
	note G_, 2
	note D_, 2
	note G_, 2
	note A#, 2
	note G_, 2
	note C#, 2
	note E_, 2
	note A_, 2
	note E_, 2
	note C#, 2
	note E_, 2
	note A_, 2
	note E_, 2
	sound_ret

Music_LottaSelvatico_Ch3::
	note_type 12, 1, 2
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
	note F_, 2
	octave 4
	note F_, 2
	octave 3
	note F_, 2
	octave 4
	note F_, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	note C#, 2
	note E_, 2

.mainloop:
	sound_call .tema
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
	note F_, 2
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
	note G_, 2
	octave 4
	note C_, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	note E_, 2
	note A_, 2
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
	note A_, 2
	octave 4
	note D_, 2
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
	note D_, 2
	note G_, 2
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
	note F_, 2
	note A#, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	note E_, 2
	note A_, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	note E_, 2
	note A_, 2
	sound_call .tema
	sound_loop 0, .mainloop

.tema:
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
	octave 3
	note D_, 2
	octave 4
	note D_, 2
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
	octave 3
	note D_, 2
	octave 4
	note D_, 2
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
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	octave 2
	note A_, 2
	octave 3
	note A_, 2
	sound_ret
