

byte getRowVals(byte r) {
	byte ib = (PAGE * 24) + (r << 2); // Get the row's contents' base array positions
	byte ib2 = ib + 1;
	byte ib3 = ib2 + 1;
	byte ib4 = ib3 + 1;
	return ((SCATTER[ib] & 7) ? 128 : 0) // Return all the row's SCATTER and PLAYING info as a row's worth of bits
		| ((SCATTER[ib2] & 7) ? 64 : 0) // ^
		| ((SCATTER[ib3] & 7) ? 32 : 0) // ^
		| ((SCATTER[ib4] & 7) ? 16 : 0) // ^
		| ((STATS[ib] & 128) >> 4) // ^
		| ((STATS[ib2] & 128) >> 5) // ^
		| ((STATS[ib3] & 128) >> 6) // ^
		| ((STATS[ib4] & 128) >> 7); // ^
}

// Update the GUI based on update-flags that have been set by the current tick's events
void updateGUI() {

	// Note: The top two LED-rows are the same in both RECORD and PLAY modes

	byte ctrl = BUTTONS & B00111111; // Get the control-row buttons' activity

	if (TO_UPDATE & 1) { // If the first row is flagged for a GUI update...
		lc.setRow(0, 0, (128 >> PAGE) | ctrl); // Display current PAGE and CTRL-commands
	}

	if (TO_UPDATE & 2) { // If the second row is flagged for a GUI update...
		if (RECORDMODE) { // If RECORD MODE is active...
			if (ctrl == B00010000) { // If BASE-NOTE is pressed...
				lc.setRow(0, 1, BASENOTE); // Display BASENOTE value
			} else if (ctrl == B00001000) { // Else, if OCTAVE is pressed...
				lc.setRow(0, 1, OCTAVE); // Display OCTAVE value
			} else if (ctrl == B00011000) { // Else, if VELO is pressed...
				lc.setRow(0, 1, VELO); // Display VELO value
			} else if (ctrl == B00000100) { // If DURATION is pressed...
				lc.setRow(0, 1, DURATION); // Display DURATION value
			} else if (ctrl == B00010100) { // Else, if HUMANIZE is pressed...
				lc.setRow(0, 1, HUMANIZE); // Display HUMANIZE value
			} else if (ctrl == B00001100) { // If QUANTIZE is pressed...
				lc.setRow(0, 1, QUANTIZE); // Display QUANTIZE value
			} else if (ctrl == B00010010) { // If CHAN is pressed...
				lc.setRow(0, 1, CHAN); // Display CHAN value
			} else if (ctrl == B00001010) { // If SEQ-SIZE is pressed...
				lc.setRow(0, 1, STATS[RECORDSEQ] & 127); // Display current record-seq's SIZE value
			} else if (ctrl == B00011010) { // If CLOCK-MASTER is pressed...
				lc.setRow(0, 1, CLOCKMASTER * 255); // Display MIDI CLOCK MASTER/FOLLOWER status
			} else if (ctrl == B00000110) { // If BPM is pressed...
				lc.setRow(0, 1, BPM); // Display BPM value
			} else if (ctrl == B00010110) { // If CHAN-LISTEN is pressed...
				lc.setRow(0, 1, LISTEN); // Display the CHAN-LISTEN value
			} else { // If no CTRL buttons are held, or an unknown or irrelevant combination of CTRL buttons are held...
				lc.setRow(0, 1, 128 >> ((CUR16 >> 4) - 1)); // Display the global-tick position
			}
		} else { // Else, if PLAYING MODE is active...
			lc.setRow(0, 1, 128 >> ((CUR16 >> 4) - 1)); // Display the global-tick position
		}

	}

	if (TO_UPDATE & 252) { // If any of the bottom 6 rows are flagged for a GUI update...

		if (RECORDMODE) { // If RECORD MODE is active...

			for (byte i = 0; i < 6; i++) { // For each of the bottom 6 GUI rows...

				if (!(TO_UPDATE & (4 << i))) { continue; } // If the row is not flagged for an update, continue to the next row

				// Holds the LED-row's contents, which will be assembled based on which commands are held
				byte row = 0;

				if (ctrl <= 1) { // If NOTE-RECORDING is being toggled, or no CTRL button is held...
					if (RECORDNOTES) { // If currently recording notes, then...
						row = pgm_read_byte_near(GLYPH_RECORDING_ARMED + i); // Grab a section of the ARMED-RECORDING-glyph for display
					} else {
						row = pgm_read_byte_near(GLYPH_RECORDING + i); // Grab a section of the RECORDING-glyph for display
					}
				} else if (ctrl == B00010000) { // If BASE-NOTE command is held...
					row = pgm_read_byte_near(GLYPH_BASENOTE + i); // Grab a section of the BASENOTE glyph for display
				} else if (ctrl == B00001000) { // If OCTAVE command is held...
					row = pgm_read_byte_near(GLYPH_OCTAVE + i); // Grab a section of the OCTAVE glyph for display
				} else if (ctrl == B00000100) { // If DURATION command is held...
					row = pgm_read_byte_near(GLYPH_DURATION + i); // Grab a section of the DURATION glyph for display
				} else if (ctrl == B00000010) { // If SWITCH-SEQ command is held...
					row = pgm_read_byte_near(GLYPH_RSWITCH + i); // Grab a section of the SWITCH-SEQ-glyph for display
				} else if (ctrl == B00000001) { // If ERASE WHILE HELD command is held...
					row = pgm_read_byte_near(GLYPH_ERASE + i); // Grab a section of the ERASE-glyph for display
				} else if (ctrl == B00011000) { // If VELO command is held...
					row = pgm_read_byte_near(GLYPH_VELO + i); // Grab a section of the VELO glyph for display
				} else if (ctrl == B00010100) { // If HUMANIZE command is held...
					row = pgm_read_byte_near(GLYPH_HUMANIZE + i); // Grab a section of the HUMANIZE glyph for display
				} else if (ctrl == B00001100) { // If QUANTIZE command is held...
					row = pgm_read_byte_near(GLYPH_QUANTIZE + i); // Grab a section of the QUANTIZE glyph for display
				} else if (ctrl == B00010010) { // If CHAN command is held...
					row = pgm_read_byte_near(GLYPH_CHAN + i); // Grab a section of the CHAN glyph for display
				} else if (ctrl == B00001010) { // If SEQ-SIZE command is held...
					row = pgm_read_byte_near(GLYPH_SIZE + i); // Grab a section of the SEQ-SIZE glyph for display
				} else if (ctrl == B00000110) { // If BPM command is held...
					row = pgm_read_byte_near(GLYPH_BPM + i); // Grab a section of the BPM glyph for display
				} else if (ctrl == B00011100) { // If SAVE command is held...
					row = pgm_read_byte_near(GLYPH_SAVE + i); // Grab a section of the SAVE-glyph for display
				} else if (ctrl == B00011010) { // If CLOCK-MASTER command is held...
					row = pgm_read_byte_near(GLYPH_CLOCKMASTER + i); // Grab a section of the CLOCK-MASTER glyph for display
				} else if (ctrl == B00010110) { // If CHAN-LISTEN command is held...
					row = pgm_read_byte_near(GLYPH_LISTEN + i); // Grab a section of the CHAN-LISTEN glyph for display
				} else if (ctrl == B00001110) { // If LOAD command is held...
					row = pgm_read_byte_near(GLYPH_LOAD + i); // Grab a section of the LOAD-glyph for display
				} else { // Else, if no command is held...
					row = getRowVals(i);
				}

				lc.setRow(0, i + 2, row); // Set the LED-row based on the current display-row

			}

		} else { // Else, if PLAYING MODE is actve...
			for (byte i = 2; i < 8; i++) { // For each of the bottom 6 GUI rows...
				if (!(TO_UPDATE & (1 << i))) { continue; } // If the row is not flagged for an update, continue to the next row
				lc.setRow(0, i, getRowVals(i - 2)); // Display the row of SCATTER and PLAYING info
			}
		}

	}

	TO_UPDATE = 0; // Unset the GUI-row-update flags

}
