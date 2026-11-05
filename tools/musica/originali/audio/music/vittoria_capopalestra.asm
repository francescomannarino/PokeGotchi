; Vittoria su un capopalestra
; Brano originale scritto per PokeGotchi, coperto dalla licenza del progetto (MIT).
; Sol maggiore, tempo 112: fanfara, marcia e coda, 24 battute; poi il brano finisce.

Music_VittoriaCapopalestra_Ch1::
	tempo 112
	volume 7, 7
	duty_cycle 2
	vibrato 12, 2, 4
	toggle_perfect_pitch
	note_type 12, 12, 5
; fanfara: G C D D
	octave 3
	note D_, 2
	note G_, 2
	note B_, 4
	octave 4
	note D_, 6
	octave 3
	note B_, 2
	octave 4
	note E_, 6
	note D_, 2
	note C_, 4
	note E_, 4
	note F#, 6
	note E_, 2
	note D_, 4
	note C_, 4
	octave 3
	note A_, 4
	octave 4
	note D_, 4
	note F#, 8
	sound_call .marcia
	note_type 12, 11, 6
; Em C D D
	note E_, 4
	note G_, 4
	note B_, 6
	note A_, 2
	note G_, 4
	note E_, 4
	note C_, 4
	note E_, 4
	note D_, 6
	note E_, 2
	note F#, 4
	note A_, 4
	note A_, 8
	note F#, 8
	sound_call .marcia
	note_type 12, 11, 6
; Em C D G, poi la coda: C D G G
	note E_, 4
	note G_, 4
	note B_, 6
	octave 5
	note C_, 2
	octave 4
	note B_, 4
	note G_, 4
	note E_, 4
	note C_, 4
	note A_, 4
	note F#, 4
	note D_, 4
	note F#, 4
	note G_, 16
	note_type 12, 12, 5
	note E_, 4
	note G_, 4
	octave 5
	note C_, 8
	octave 4
	note F#, 4
	note A_, 4
	octave 5
	note D_, 8
	note D_, 4
	octave 4
	note B_, 4
	note G_, 8
	note_type 12, 11, 7
	note G_, 12
	rest 4
	sound_ret

; marcia: G C D G
.marcia:
	note_type 12, 12, 3
	octave 4
	note G_, 4
	note D_, 2
	octave 3
	note B_, 2
	note G_, 4
	note B_, 4
	octave 4
	note C_, 4
	note E_, 4
	note G_, 6
	note E_, 2
	note F#, 4
	note E_, 2
	note D_, 2
	octave 3
	note A_, 4
	note F#, 4
	note G_, 4
	note B_, 4
	octave 4
	note D_, 8
	sound_ret

Music_VittoriaCapopalestra_Ch2::
	duty_cycle 1
	vibrato 10, 1, 4
	note_type 12, 10, 5
	octave 2
	note B_, 4
	octave 3
	note D_, 4
	note G_, 4
	note B_, 4
	octave 4
	note C_, 8
	octave 3
	note G_, 4
	octave 4
	note C_, 4
	note D_, 6
	note C_, 2
	octave 3
	note A_, 4
	note F#, 4
	note F#, 4
	note A_, 4
	octave 4
	note D_, 8
	sound_call .marcia
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
	note A_, 2
	rest 2
	note F#, 2
	rest 2
	note A_, 2
	sound_call .marcia
	note_type 12, 8, 1
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
	note F#, 2
	rest 2
	note A_, 2
	rest 2
	note F#, 2
	rest 2
	note A_, 2
	rest 2
	note G_, 2
	rest 2
	note B_, 2
	rest 2
	note G_, 2
	rest 2
	note B_, 2
	note_type 12, 10, 6
	octave 4
	note C_, 8
	note E_, 8
	note D_, 8
	note F#, 8
	octave 3
	note B_, 8
	octave 4
	note D_, 8
	octave 3
	note B_, 12
	rest 4
	sound_ret

.marcia:
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
	note G_, 2
	rest 2
	note B_, 2
	rest 2
	note G_, 2
	rest 2
	note B_, 2
	sound_ret

Music_VittoriaCapopalestra_Ch3::
	note_type 12, 1, 2
	octave 2
	note G_, 8
	note G_, 8
	octave 3
	note C_, 8
	note C_, 8
	note D_, 8
	note D_, 8
	note D_, 8
	note D_, 8
	sound_call .marcia
	note E_, 2
	rest 2
	note B_, 2
	rest 2
	note E_, 2
	rest 2
	note B_, 2
	rest 2
	note C_, 2
	rest 2
	note G_, 2
	rest 2
	note C_, 2
	rest 2
	note G_, 2
	rest 2
	note D_, 2
	rest 2
	note A_, 2
	rest 2
	note D_, 2
	rest 2
	note A_, 2
	rest 2
	note D_, 2
	rest 2
	note A_, 2
	rest 2
	note D_, 2
	rest 2
	note A_, 2
	rest 2
	sound_call .marcia
	note E_, 2
	rest 2
	note B_, 2
	rest 2
	note E_, 2
	rest 2
	note B_, 2
	rest 2
	note C_, 2
	rest 2
	note G_, 2
	rest 2
	note C_, 2
	rest 2
	note G_, 2
	rest 2
	note D_, 2
	rest 2
	note A_, 2
	rest 2
	note D_, 2
	rest 2
	note A_, 2
	rest 2
	octave 2
	note G_, 2
	rest 2
	octave 3
	note D_, 2
	rest 2
	octave 2
	note G_, 2
	rest 2
	octave 3
	note D_, 2
	rest 2
	note C_, 8
	note E_, 8
	note D_, 8
	note F#, 8
	octave 2
	note G_, 8
	note B_, 8
	note G_, 12
	rest 4
	sound_ret

.marcia:
	octave 2
	note G_, 2
	rest 2
	octave 3
	note D_, 2
	rest 2
	octave 2
	note G_, 2
	rest 2
	octave 3
	note D_, 2
	rest 2
	note C_, 2
	rest 2
	note G_, 2
	rest 2
	note C_, 2
	rest 2
	note G_, 2
	rest 2
	note D_, 2
	rest 2
	note A_, 2
	rest 2
	note D_, 2
	rest 2
	note A_, 2
	rest 2
	octave 2
	note G_, 2
	rest 2
	octave 3
	note D_, 2
	rest 2
	octave 2
	note G_, 2
	rest 2
	octave 3
	note D_, 2
	rest 2
	sound_ret
