; Lotta finale
; Brano originale scritto per PokeGotchi, coperto dalla licenza del progetto (MIT).
; Do minore, tempo 112: introduzione di 4 battute, giro A B A C di 32 battute.

Music_LottaFinale_Ch1::
	tempo 112
	volume 7, 7
	duty_cycle 3
	vibrato 10, 2, 5
	toggle_perfect_pitch
	note_type 12, 11, 1
; introduzione: arpeggi discendenti di Do minore, Sib, Lab, poi Sol7
	octave 5
	note C_, 1
	octave 4
	note G_, 1
	note D#, 1
	note C_, 1
	octave 5
	note C_, 1
	octave 4
	note G_, 1
	note D#, 1
	note C_, 1
	octave 5
	note C_, 1
	octave 4
	note G_, 1
	note D#, 1
	note C_, 1
	octave 5
	note C_, 1
	octave 4
	note G_, 1
	note D#, 1
	note C_, 1
	octave 5
	note D_, 1
	octave 4
	note A#, 1
	note F_, 1
	note D_, 1
	octave 5
	note D_, 1
	octave 4
	note A#, 1
	note F_, 1
	note D_, 1
	octave 5
	note D_, 1
	octave 4
	note A#, 1
	note F_, 1
	note D_, 1
	octave 5
	note D_, 1
	octave 4
	note A#, 1
	note F_, 1
	note D_, 1
	octave 5
	note D#, 1
	note C_, 1
	octave 4
	note G#, 1
	note D#, 1
	octave 5
	note D#, 1
	note C_, 1
	octave 4
	note G#, 1
	note D#, 1
	octave 5
	note D#, 1
	note C_, 1
	octave 4
	note G#, 1
	note D#, 1
	octave 5
	note D#, 1
	note C_, 1
	octave 4
	note G#, 1
	note D#, 1
	note_type 12, 12, 3
	note G_, 4
	note B_, 4
	octave 5
	note D_, 4
	note F_, 4

.mainloop:
	sound_call .a
	note_type 12, 12, 3
; B: Eb Bb Cm Ab Fm Gm Ab G7
	octave 5
	note D#, 2
	rest 2
	note D#, 2
	note D_, 2
	note D#, 4
	note G_, 4
	note F_, 2
	rest 2
	note F_, 2
	note D#, 2
	note D_, 4
	octave 4
	note A#, 4
	octave 5
	note C_, 2
	rest 2
	note C_, 2
	note D_, 2
	note D#, 4
	octave 4
	note G_, 4
	note_type 12, 11, 7
	note G#, 12
	note G_, 2
	note G#, 2
	octave 5
	note C_, 6
	octave 4
	note G#, 2
	note F_, 8
	octave 5
	note D_, 4
	octave 4
	note A#, 4
	note G_, 8
	octave 5
	note D#, 6
	note C_, 2
	octave 4
	note G#, 4
	octave 5
	note C_, 4
	octave 4
	note B_, 4
	octave 5
	note D_, 4
	note G_, 8
	sound_call .a
	note_type 12, 12, 4
; C: Fm G7 Cm Cm Fm Bb Eb G7
	octave 4
	note F_, 2
	note G#, 2
	octave 5
	note C_, 2
	note F_, 2
	note D#, 4
	note C_, 4
	note D_, 2
	octave 4
	note B_, 2
	note G_, 2
	note B_, 2
	octave 5
	note D_, 8
	note_type 12, 11, 7
	note D#, 8
	note G_, 8
	note C_, 16
	note_type 12, 12, 4
	octave 4
	note G#, 2
	octave 5
	note C_, 2
	note F_, 2
	note G#, 2
	note G_, 4
	note F_, 4
	octave 4
	note A#, 2
	octave 5
	note D_, 2
	note F_, 2
	note D_, 2
	octave 4
	note A#, 8
	note G_, 4
	note A#, 4
	octave 5
	note D#, 8
	note D_, 4
	note C_, 4
	octave 4
	note B_, 4
	note G_, 4
	sound_loop 0, .mainloop

; A: Cm Ab Bb G7 Cm Ab Fm G7
.a:
	note_type 12, 12, 5
	octave 4
	note G_, 4
	octave 5
	note C_, 4
	note D#, 6
	note D_, 2
	note C_, 4
	octave 4
	note G#, 4
	note D#, 8
	octave 5
	note D_, 4
	octave 4
	note A#, 2
	note F_, 2
	note A#, 4
	octave 5
	note D_, 4
	octave 4
	note B_, 12
	note G_, 4
	note G_, 4
	octave 5
	note C_, 4
	note D#, 6
	note F_, 2
	note G_, 6
	note F_, 2
	note D#, 4
	note C_, 4
	octave 4
	note G#, 4
	octave 5
	note C_, 4
	note F_, 4
	note D#, 2
	note D_, 2
	note D_, 8
	octave 4
	note B_, 4
	note G_, 4
	sound_ret

Music_LottaFinale_Ch2::
	duty_cycle 1
	vibrato 8, 1, 4
	note_type 12, 10, 7
	octave 4
	note C_, 16
	octave 3
	note A#, 16
	note G#, 16
	octave 4
	note D_, 8
	octave 3
	note B_, 8

.mainloop:
	sound_call .a
	note_type 12, 8, 1
	rest 2
	octave 3
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
	note D#, 2
	rest 2
	note G_, 2
	rest 2
	note D#, 2
	rest 2
	note G_, 2
	rest 2
	note D#, 2
	rest 2
	note G#, 2
	rest 2
	note D#, 2
	rest 2
	note G#, 2
	rest 2
	note F_, 2
	rest 2
	note G#, 2
	rest 2
	note F_, 2
	rest 2
	note G#, 2
	rest 2
	note G_, 2
	rest 2
	note A#, 2
	rest 2
	note G_, 2
	rest 2
	note A#, 2
	rest 2
	note D#, 2
	rest 2
	note G#, 2
	rest 2
	note D#, 2
	rest 2
	note G#, 2
	rest 2
	note F_, 2
	rest 2
	note B_, 2
	rest 2
	note F_, 2
	rest 2
	note B_, 2
	sound_call .a
	note_type 12, 9, 3
	note C_, 2
	note F_, 2
	note G#, 2
	note F_, 2
	note C_, 2
	note F_, 2
	note G#, 2
	note F_, 2
	note D_, 2
	note F_, 2
	note B_, 2
	note F_, 2
	note D_, 2
	note F_, 2
	note B_, 2
	note F_, 2
	note C_, 2
	note D#, 2
	note G_, 2
	note D#, 2
	note C_, 2
	note D#, 2
	note G_, 2
	note D#, 2
	note C_, 2
	note D#, 2
	note G_, 2
	note D#, 2
	note C_, 2
	note D#, 2
	note G_, 2
	note D#, 2
	note C_, 2
	note F_, 2
	note G#, 2
	note F_, 2
	note C_, 2
	note F_, 2
	note G#, 2
	note F_, 2
	note D_, 2
	note F_, 2
	note A#, 2
	note F_, 2
	note D_, 2
	note F_, 2
	note A#, 2
	note F_, 2
	note D#, 2
	note G_, 2
	note A#, 2
	note G_, 2
	note D#, 2
	note G_, 2
	note A#, 2
	note G_, 2
	note D_, 2
	note F_, 2
	note B_, 2
	note F_, 2
	note D_, 2
	note F_, 2
	note B_, 2
	note F_, 2
	sound_loop 0, .mainloop

.a:
	note_type 12, 8, 1
	octave 3
	note C_, 1
	note D#, 1
	note G_, 1
	note D#, 1
	note C_, 1
	note D#, 1
	note G_, 1
	note D#, 1
	note C_, 1
	note D#, 1
	note G_, 1
	note D#, 1
	note C_, 1
	note D#, 1
	note G_, 1
	note D#, 1
	note C_, 1
	note D#, 1
	note G#, 1
	note D#, 1
	note C_, 1
	note D#, 1
	note G#, 1
	note D#, 1
	note C_, 1
	note D#, 1
	note G#, 1
	note D#, 1
	note C_, 1
	note D#, 1
	note G#, 1
	note D#, 1
	note D_, 1
	note F_, 1
	note A#, 1
	note F_, 1
	note D_, 1
	note F_, 1
	note A#, 1
	note F_, 1
	note D_, 1
	note F_, 1
	note A#, 1
	note F_, 1
	note D_, 1
	note F_, 1
	note A#, 1
	note F_, 1
	note D_, 1
	note F_, 1
	note B_, 1
	note F_, 1
	note D_, 1
	note F_, 1
	note B_, 1
	note F_, 1
	note D_, 1
	note F_, 1
	note B_, 1
	note F_, 1
	note D_, 1
	note F_, 1
	note B_, 1
	note F_, 1
	note C_, 1
	note D#, 1
	note G_, 1
	note D#, 1
	note C_, 1
	note D#, 1
	note G_, 1
	note D#, 1
	note C_, 1
	note D#, 1
	note G_, 1
	note D#, 1
	note C_, 1
	note D#, 1
	note G_, 1
	note D#, 1
	note C_, 1
	note D#, 1
	note G#, 1
	note D#, 1
	note C_, 1
	note D#, 1
	note G#, 1
	note D#, 1
	note C_, 1
	note D#, 1
	note G#, 1
	note D#, 1
	note C_, 1
	note D#, 1
	note G#, 1
	note D#, 1
	note C_, 1
	note F_, 1
	note G#, 1
	note F_, 1
	note C_, 1
	note F_, 1
	note G#, 1
	note F_, 1
	note C_, 1
	note F_, 1
	note G#, 1
	note F_, 1
	note C_, 1
	note F_, 1
	note G#, 1
	note F_, 1
	note D_, 1
	note F_, 1
	note B_, 1
	note F_, 1
	note D_, 1
	note F_, 1
	note B_, 1
	note F_, 1
	note D_, 1
	note F_, 1
	note B_, 1
	note F_, 1
	note D_, 1
	note F_, 1
	note B_, 1
	note F_, 1
	sound_ret

Music_LottaFinale_Ch3::
	note_type 12, 1, 2
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
	octave 2
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note G#, 2
	octave 3
	note G#, 2
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

.mainloop:
	sound_call .a
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
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note G#, 2
	octave 3
	note G#, 2
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
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note G#, 2
	octave 3
	note G#, 2
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
	sound_call .a
	octave 2
	note F_, 4
	rest 2
	note F_, 2
	octave 3
	note F_, 4
	octave 2
	note F_, 4
	note G_, 4
	rest 2
	note G_, 2
	octave 3
	note G_, 4
	octave 2
	note G_, 4
	octave 3
	note C_, 4
	rest 2
	note C_, 2
	octave 4
	note C_, 4
	octave 3
	note C_, 4
	note C_, 4
	rest 2
	note C_, 2
	octave 4
	note C_, 4
	octave 3
	note C_, 4
	octave 2
	note F_, 4
	rest 2
	note F_, 2
	octave 3
	note F_, 4
	octave 2
	note F_, 4
	note A#, 4
	rest 2
	note A#, 2
	octave 3
	note A#, 4
	octave 2
	note A#, 4
	octave 3
	note D#, 4
	rest 2
	note D#, 2
	octave 4
	note D#, 4
	octave 3
	note D#, 4
	octave 2
	note G_, 4
	rest 2
	note G_, 2
	octave 3
	note G_, 4
	octave 2
	note G_, 4
	sound_loop 0, .mainloop

.a:
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
	note C_, 2
	octave 4
	note C_, 2
	octave 2
	note G#, 2
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note G#, 2
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note A#, 2
	note A#, 2
	octave 3
	note A#, 2
	octave 2
	note A#, 2
	note A#, 2
	octave 3
	note A#, 2
	octave 2
	note A#, 2
	octave 3
	note A#, 2
	octave 2
	note G_, 2
	note G_, 2
	octave 3
	note G_, 2
	octave 2
	note G_, 2
	note G_, 2
	octave 3
	note G_, 2
	octave 2
	note G_, 2
	octave 3
	note G_, 2
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
	note C_, 2
	octave 4
	note C_, 2
	octave 2
	note G#, 2
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note G#, 2
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note G#, 2
	octave 3
	note G#, 2
	octave 2
	note F_, 2
	note F_, 2
	octave 3
	note F_, 2
	octave 2
	note F_, 2
	note F_, 2
	octave 3
	note F_, 2
	octave 2
	note F_, 2
	octave 3
	note F_, 2
	octave 2
	note G_, 2
	note G_, 2
	octave 3
	note G_, 2
	octave 2
	note G_, 2
	note G_, 2
	octave 3
	note G_, 2
	octave 2
	note G_, 2
	octave 3
	note G_, 2
	sound_ret
