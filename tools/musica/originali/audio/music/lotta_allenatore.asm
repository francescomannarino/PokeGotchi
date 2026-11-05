; Lotta contro un allenatore
; Brano originale scritto per PokeGotchi, coperto dalla licenza del progetto (MIT).
; Mi minore, tempo 112: introduzione di 4 battute, giro A B A C di 32 battute.

Music_LottaAllenatore_Ch1::
	tempo 112
	volume 7, 7
	duty_cycle 2
	vibrato 8, 2, 5
	toggle_perfect_pitch
	note_type 12, 12, 3
; introduzione: colpi sincopati 3+3+2 che salgono, poi il Si di dominante
	octave 4
	note E_, 3
	note E_, 3
	note E_, 2
	rest 4
	note D_, 2
	note E_, 2
	note G_, 3
	note G_, 3
	note G_, 2
	rest 4
	note F#, 2
	note G_, 2
	note A_, 3
	note A_, 3
	note A_, 2
	rest 4
	note B_, 2
	note A_, 2
	note B_, 8
	rest 4
	octave 3
	note B_, 4

.mainloop:
	sound_call .a
	note_type 12, 11, 6
; B: Am D G C Am B Em Em
	octave 4
	note A_, 6
	octave 5
	note C_, 2
	note E_, 8
	note D_, 6
	note C_, 2
	octave 4
	note A_, 4
	note F#, 4
	note G_, 6
	note A_, 2
	note B_, 4
	octave 5
	note D_, 4
	note E_, 8
	note D_, 4
	note C_, 4
	octave 4
	note A_, 6
	note B_, 2
	octave 5
	note C_, 4
	note E_, 4
	octave 4
	note F#, 4
	note A_, 4
	octave 5
	note D#, 4
	octave 4
	note B_, 4
	octave 5
	note E_, 12
	note D_, 2
	octave 4
	note B_, 2
	note G_, 4
	note F#, 4
	note E_, 4
	note D#, 4
	sound_call .a
	note_type 12, 12, 2
; C: C D Em Em C D B B
	note G_, 2
	rest 2
	note G_, 2
	rest 2
	note E_, 4
	note G_, 4
	note A_, 2
	rest 2
	note A_, 2
	rest 2
	note F#, 4
	note A_, 4
	note B_, 2
	rest 2
	note B_, 2
	rest 2
	note G_, 4
	note B_, 4
	note_type 12, 11, 7
	octave 5
	note E_, 16
	note_type 12, 12, 2
	note E_, 2
	rest 2
	note E_, 2
	rest 2
	note C_, 4
	note E_, 4
	note D_, 2
	rest 2
	note D_, 2
	rest 2
	octave 4
	note A_, 4
	octave 5
	note D_, 4
	note_type 12, 11, 6
	note D#, 8
	octave 4
	note B_, 8
	note F#, 4
	note A_, 4
	note B_, 4
	octave 5
	note D#, 4
	sound_loop 0, .mainloop

; A: Em C D B Em C Am B
.a:
	note_type 12, 12, 4
	octave 4
	note B_, 4
	note G_, 2
	note E_, 2
	rest 2
	note E_, 2
	note F#, 2
	note G_, 2
	note A_, 4
	note G_, 2
	note E_, 2
	rest 2
	note C_, 2
	note D_, 2
	note E_, 2
	note F#, 6
	note A_, 2
	octave 5
	note D_, 4
	note C_, 2
	octave 4
	note A_, 2
	note B_, 12
	note D#, 2
	note F#, 2
	note B_, 4
	note G_, 2
	note E_, 2
	rest 2
	note E_, 2
	note F#, 2
	note G_, 2
	octave 5
	note C_, 4
	octave 4
	note B_, 2
	note G_, 2
	rest 2
	note E_, 2
	note G_, 2
	octave 5
	note C_, 2
	note E_, 6
	note D_, 2
	note C_, 4
	octave 4
	note A_, 4
	note B_, 8
	note A_, 4
	note F#, 4
	sound_ret

Music_LottaAllenatore_Ch2::
	duty_cycle 3
	vibrato 6, 1, 4
	note_type 12, 10, 3
	octave 3
	note B_, 3
	note B_, 3
	note B_, 2
	rest 4
	note A_, 2
	note B_, 2
	octave 4
	note D_, 3
	note D_, 3
	note D_, 2
	rest 4
	note C_, 2
	note D_, 2
	note E_, 3
	note E_, 3
	note E_, 2
	rest 4
	note F#, 2
	note E_, 2
	note D#, 8
	rest 4
	octave 3
	note F#, 4

.mainloop:
	sound_call .a
	note_type 12, 9, 7
	octave 4
	note E_, 8
	note C_, 8
	note F#, 6
	note E_, 2
	note D_, 8
	note D_, 8
	octave 3
	note B_, 8
	octave 4
	note C_, 4
	note E_, 4
	note G_, 8
	octave 3
	note A_, 8
	octave 4
	note C_, 8
	octave 3
	note B_, 6
	octave 4
	note D#, 2
	note F#, 8
	note G_, 8
	note E_, 8
	octave 3
	note B_, 8
	octave 4
	note D#, 8
	sound_call .a
	note_type 12, 9, 2
	octave 3
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
	note F#, 2
	note A_, 2
	note F#, 2
	note D_, 2
	note F#, 2
	note A_, 2
	note F#, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note G_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note G_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note G_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note G_, 2
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
	note F#, 2
	note A_, 2
	note F#, 2
	note D_, 2
	note F#, 2
	note A_, 2
	note F#, 2
	note D#, 2
	note F#, 2
	note B_, 2
	note F#, 2
	note D#, 2
	note F#, 2
	note B_, 2
	note F#, 2
	note D#, 2
	note F#, 2
	note B_, 2
	note F#, 2
	note D#, 2
	note F#, 2
	note B_, 2
	note F#, 2
	sound_loop 0, .mainloop

.a:
	note_type 12, 8, 1
	rest 2
	octave 3
	note G_, 2
	rest 2
	note B_, 2
	rest 2
	note G_, 2
	rest 2
	note B_, 2
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
	note B_, 2
	rest 2
	note F#, 2
	rest 2
	note B_, 2
	rest 2
	note G_, 2
	rest 2
	note B_, 2
	rest 2
	note G_, 2
	rest 2
	note B_, 2
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
	note F#, 2
	rest 2
	note B_, 2
	rest 2
	note F#, 2
	rest 2
	note B_, 2
	sound_ret

Music_LottaAllenatore_Ch3::
	note_type 12, 1, 2
	octave 3
	note E_, 3
	note E_, 3
	note E_, 2
	rest 4
	note E_, 2
	note E_, 2
	note C_, 3
	note C_, 3
	note C_, 2
	rest 4
	note C_, 2
	note C_, 2
	note D_, 3
	note D_, 3
	note D_, 2
	rest 4
	note D_, 2
	note D_, 2
	octave 2
	note B_, 8
	rest 4
	note B_, 4

.mainloop:
	sound_call .a
	octave 2
	note A_, 4
	note A_, 2
	octave 3
	note A_, 2
	octave 2
	note A_, 4
	octave 3
	note A_, 4
	note D_, 4
	note D_, 2
	octave 4
	note D_, 2
	octave 3
	note D_, 4
	octave 4
	note D_, 4
	octave 2
	note G_, 4
	note G_, 2
	octave 3
	note G_, 2
	octave 2
	note G_, 4
	octave 3
	note G_, 4
	note C_, 4
	note C_, 2
	octave 4
	note C_, 2
	octave 3
	note C_, 4
	octave 4
	note C_, 4
	octave 2
	note A_, 4
	note A_, 2
	octave 3
	note A_, 2
	octave 2
	note A_, 4
	octave 3
	note A_, 4
	octave 2
	note B_, 4
	note B_, 2
	octave 3
	note B_, 2
	octave 2
	note B_, 4
	octave 3
	note B_, 4
	note E_, 4
	note E_, 2
	octave 4
	note E_, 2
	octave 3
	note E_, 4
	octave 4
	note E_, 4
	octave 3
	note E_, 4
	note E_, 2
	octave 4
	note E_, 2
	octave 3
	note E_, 4
	octave 4
	note E_, 4
	sound_call .a
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
	note E_, 2
	octave 4
	note E_, 2
	octave 3
	note E_, 2
	octave 4
	note E_, 2
	octave 3
	note E_, 2
	octave 4
	note E_, 2
	octave 3
	note E_, 2
	octave 4
	note E_, 2
	octave 3
	note E_, 2
	octave 4
	note E_, 2
	octave 3
	note E_, 2
	octave 4
	note E_, 2
	octave 3
	note E_, 2
	octave 4
	note E_, 2
	octave 3
	note E_, 2
	octave 4
	note E_, 2
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
	octave 2
	note B_, 2
	octave 3
	note B_, 2
	octave 2
	note B_, 2
	octave 3
	note B_, 2
	octave 2
	note B_, 2
	octave 3
	note B_, 2
	octave 2
	note B_, 2
	octave 3
	note B_, 2
	octave 2
	note B_, 2
	octave 3
	note B_, 2
	octave 2
	note B_, 2
	octave 3
	note B_, 2
	octave 2
	note B_, 2
	octave 3
	note B_, 2
	octave 2
	note B_, 2
	octave 3
	note B_, 2
	sound_loop 0, .mainloop

.a:
	octave 3
	note E_, 2
	note E_, 2
	octave 4
	note E_, 2
	octave 3
	note E_, 2
	note E_, 2
	octave 4
	note E_, 2
	octave 3
	note B_, 2
	octave 4
	note E_, 2
	octave 3
	note C_, 2
	note C_, 2
	octave 4
	note C_, 2
	octave 3
	note C_, 2
	note C_, 2
	octave 4
	note C_, 2
	octave 3
	note G_, 2
	octave 4
	note C_, 2
	octave 3
	note D_, 2
	note D_, 2
	octave 4
	note D_, 2
	octave 3
	note D_, 2
	note D_, 2
	octave 4
	note D_, 2
	octave 3
	note A_, 2
	octave 4
	note D_, 2
	octave 2
	note B_, 2
	note B_, 2
	octave 3
	note B_, 2
	octave 2
	note B_, 2
	note B_, 2
	octave 3
	note B_, 2
	note F#, 2
	note B_, 2
	note E_, 2
	note E_, 2
	octave 4
	note E_, 2
	octave 3
	note E_, 2
	note E_, 2
	octave 4
	note E_, 2
	octave 3
	note B_, 2
	octave 4
	note E_, 2
	octave 3
	note C_, 2
	note C_, 2
	octave 4
	note C_, 2
	octave 3
	note C_, 2
	note C_, 2
	octave 4
	note C_, 2
	octave 3
	note G_, 2
	octave 4
	note C_, 2
	octave 2
	note A_, 2
	note A_, 2
	octave 3
	note A_, 2
	octave 2
	note A_, 2
	note A_, 2
	octave 3
	note A_, 2
	note E_, 2
	note A_, 2
	octave 2
	note B_, 2
	note B_, 2
	octave 3
	note B_, 2
	octave 2
	note B_, 2
	note B_, 2
	octave 3
	note B_, 2
	note F#, 2
	note B_, 2
	sound_ret
