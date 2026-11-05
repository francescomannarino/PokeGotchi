; Grotta
; Brano originale scritto per PokeGotchi, coperto dalla licenza del progetto (MIT).
; Mi minore, tempo 144: giro di 24 battute (A B C) su un ostinato del canale 2; batteria del canale 4 in un giro di 2 battute.

Music_Grotta_Ch1::
	tempo 144
	volume 7, 7
	duty_cycle 2
	vibrato 16, 2, 3
	toggle_perfect_pitch

.mainloop:
	note_type 12, 10, 7
; A: Em F Em D C B Em Em
	rest 8
	octave 3
	note B_, 4
	octave 4
	note E_, 4
	note F_, 8
	note E_, 4
	note C_, 4
	octave 3
	note B_, 12
	rest 4
	note A_, 4
	octave 4
	note D_, 4
	note F#, 4
	note E_, 4
	note E_, 8
	note G_, 4
	note F#, 4
	note F#, 12
	note D#, 4
	note E_, 16
	rest 16
; B: C D Em Em C D B B
	note G_, 6
	note F#, 2
	note E_, 8
	note F#, 6
	note E_, 2
	note D_, 8
	note E_, 4
	octave 3
	note B_, 4
	note G_, 8
	rest 8
	note G_, 4
	note A_, 4
	octave 4
	note C_, 8
	note E_, 4
	note G_, 4
	note A_, 8
	note F#, 4
	note D_, 4
	note D#, 12
	note F#, 4
	octave 3
	note B_, 16
; C: Em F Em F Em F Em B, quasi solo l'ostinato
	rest 16
	rest 16
	rest 16
	rest 16
	note_type 12, 8, 7
	octave 4
	note E_, 16
	note F_, 16
	note E_, 16
	rest 16
	sound_loop 0, .mainloop

Music_Grotta_Ch2::
	duty_cycle 1
	vibrato 0, 1, 2

.mainloop:
	note_type 12, 6, 2
	octave 3
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note C_, 2
	note F_, 2
	note A_, 2
	note C_, 2
	note F_, 2
	note A_, 2
	note C_, 2
	note F_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note D_, 2
	note F#, 2
	note A_, 2
	note D_, 2
	note F#, 2
	note A_, 2
	note D_, 2
	note F#, 2
	note E_, 2
	note G_, 2
	octave 4
	note C_, 2
	octave 3
	note E_, 2
	note G_, 2
	octave 4
	note C_, 2
	octave 3
	note E_, 2
	note G_, 2
	note D#, 2
	note F#, 2
	note B_, 2
	note D#, 2
	note F#, 2
	note B_, 2
	note D#, 2
	note F#, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note E_, 2
	note G_, 2
	octave 4
	note C_, 2
	octave 3
	note E_, 2
	note G_, 2
	octave 4
	note C_, 2
	octave 3
	note E_, 2
	note G_, 2
	note D_, 2
	note F#, 2
	note A_, 2
	note D_, 2
	note F#, 2
	note A_, 2
	note D_, 2
	note F#, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note E_, 2
	note G_, 2
	octave 4
	note C_, 2
	octave 3
	note E_, 2
	note G_, 2
	octave 4
	note C_, 2
	octave 3
	note E_, 2
	note G_, 2
	note D_, 2
	note F#, 2
	note A_, 2
	note D_, 2
	note F#, 2
	note A_, 2
	note D_, 2
	note F#, 2
	note D#, 2
	note F#, 2
	note B_, 2
	note D#, 2
	note F#, 2
	note B_, 2
	note D#, 2
	note F#, 2
	note D#, 2
	note F#, 2
	note B_, 2
	note D#, 2
	note F#, 2
	note B_, 2
	note D#, 2
	note F#, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note C_, 2
	note F_, 2
	note A_, 2
	note C_, 2
	note F_, 2
	note A_, 2
	note C_, 2
	note F_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note C_, 2
	note F_, 2
	note A_, 2
	note C_, 2
	note F_, 2
	note A_, 2
	note C_, 2
	note F_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note C_, 2
	note F_, 2
	note A_, 2
	note C_, 2
	note F_, 2
	note A_, 2
	note C_, 2
	note F_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note B_, 2
	note E_, 2
	note G_, 2
	note D#, 2
	note F#, 2
	note B_, 2
	note D#, 2
	note F#, 2
	note B_, 2
	note D#, 2
	note F#, 2
	sound_loop 0, .mainloop

Music_Grotta_Ch3::
	note_type 12, 2, 1

.mainloop:
	octave 3
	note E_, 10
	rest 2
	note E_, 2
	note B_, 2
	octave 2
	note F_, 10
	rest 2
	note F_, 2
	octave 3
	note C_, 2
	note E_, 10
	rest 2
	note E_, 2
	note B_, 2
	note D_, 10
	rest 2
	note D_, 2
	note A_, 2
	note C_, 10
	rest 2
	note C_, 2
	note G_, 2
	octave 2
	note B_, 10
	rest 2
	note B_, 2
	octave 3
	note F#, 2
	note E_, 10
	rest 2
	note E_, 2
	note B_, 2
	note E_, 10
	rest 2
	note E_, 2
	note B_, 2
	note C_, 10
	rest 2
	note C_, 2
	note G_, 2
	note D_, 10
	rest 2
	note D_, 2
	note A_, 2
	note E_, 10
	rest 2
	note E_, 2
	note B_, 2
	note E_, 10
	rest 2
	note E_, 2
	note B_, 2
	note C_, 10
	rest 2
	note C_, 2
	note G_, 2
	note D_, 10
	rest 2
	note D_, 2
	note A_, 2
	octave 2
	note B_, 10
	rest 2
	note B_, 2
	octave 3
	note F#, 2
	octave 2
	note B_, 10
	rest 2
	note B_, 2
	octave 3
	note F#, 2
	note E_, 10
	rest 2
	note E_, 2
	note B_, 2
	octave 2
	note F_, 10
	rest 2
	note F_, 2
	octave 3
	note C_, 2
	note E_, 10
	rest 2
	note E_, 2
	note B_, 2
	octave 2
	note F_, 10
	rest 2
	note F_, 2
	octave 3
	note C_, 2
	note E_, 10
	rest 2
	note E_, 2
	note B_, 2
	octave 2
	note F_, 10
	rest 2
	note F_, 2
	octave 3
	note C_, 2
	note E_, 10
	rest 2
	note E_, 2
	note B_, 2
	octave 2
	note B_, 10
	rest 2
	note B_, 2
	octave 3
	note F#, 2
	sound_loop 0, .mainloop

Music_Grotta_Ch4::
	drum_speed 12

.mainloop:
	drum_note 1, 4
	drum_note 3, 2
	drum_note 3, 2
	drum_note 2, 4
	drum_note 3, 4
	drum_note 1, 4
	drum_note 3, 2
	drum_note 1, 2
	drum_note 2, 4
	drum_note 3, 2
	drum_note 4, 2
	sound_loop 0, .mainloop
