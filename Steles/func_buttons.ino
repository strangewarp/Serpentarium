
// Parse a slice-keypress that was generated while in SLICE or OVERRVIEW mode
void parsePlayPress(byte col, byte row) {

	byte seq = col + (row * 4) + (PAGE * 24); // Get the sequence that corresponds to the given column and row
	byte ctrl = BUTTONS & B00111111; // Get the control-row buttons' activity
	byte pg = ctrl & B00000001; // Get the PAGE button's status
	byte cue = ctrl & B00000010; // Get the CUE command's status
	byte nums = ctrl & B00011100; // Get the NUMBER commands' status
	byte off = ctrl & B00100000; // Get the OFF command's status

	RECORDSEQ = seq; // Set the most-recent-seq to the seq whose button was pressed, regardless of command

	// Flip the number-values into the expected format for CUE/SLICE operations
	// (1 in lowest bit, 2 next, then 4)
	nums = (((nums & 16) >> 2) | ((nums & 4) << 2) | (nums & 8)) >> 2;

	if (ctrl == B00000101) { // If PAGE A is held...
		PAGE = 0; // Toggle to page A
	} else if (ctrl == B00001001) { // If PAGE B is held...
		PAGE = 1; // Toggle to page B
	} else if (ctrl == B00100001) { // If PAGE-OFF is held, and a regular button-press was made to signal intent...
		byte ptop = PAGE * 24; // Get the position of the first seq on the current page
		for (byte i = ptop; i < (ptop + 24); i++) { // For every seq on this page...
			resetSeq(i); // Reset the seq's contents
		}
		TO_UPDATE |= 252; // Flag the bottom 6 LED-rows for an update
	} else if (ctrl == B00000011) { // If SCATTER UNSET is held...
		SCATTER[seq] = 0; // Unset the seq's SCATTER flags
		TO_UPDATE |= 4 << row; // Flag the seq's corresponding LED-row for an update
	} else if (ctrl == B00100011) { // If PAGE-SCATTER-UNSET is held, and a regular button-press was made to signal intent...
		byte ptop = PAGE * 24; // Get the position of the first seq on the current page
		for (byte i = ptop; i < (ptop + 24); i++) { // For every seq on this page...
			SCATTER[i] = 0; // Unset the seq's SCATTER flags
		}
		TO_UPDATE |= 252; // Flag the bottom 6 LED-rows for an update
	} else if (pg && cue && nums && (!off)) { // If this is a SCATTER command of any kind...
		SCATTER[seq] = nums; // Turn this command's numeric value into the seq's SCATTER-chance flags
		TO_UPDATE |= 4 << row; // Flag the seq's corresponding LED-row for an update
	} else if (cue | off) { // If either a CUE or OFF command is present...
		if (cue | nums) { // If there is a CUE or any NUMBERS...
			CMD[seq] = (nums << 5) | ((!off) << 1) | (!!off); // Cue an on-or-off command at the given global time
		} else { // Else, if there is neither CUE nor NUMBERS, that means this is an un-cued OFF command, so...
			if (STATS[seq] & 128) { // If the sequence is playing...
				TO_UPDATE |= 4 << row; // Flag the seq's corresponding LED-row for an update
			}
			resetSeq(seq); // Reset the sequence
		}
	} else { // Else, if no special command is present...
		if (CMD[seq] & B00000010) { // If the sequence already contains a cued ON value...
			CMD[seq] = (CMD[seq] & B11100011) | (nums << 2); // Add a slice-position to the cued ON command
		} else { // Else, if the sequence has no cued ON value, then this is a pure beatslice, so...

			// Get the number of 16th-notes in one of the seq's slice-chunks
			byte csize = (STATS[seq] & 127) * 2;

			// Set the sequence's position to:
			//   A base-position equal to the slice-numbers being held,
			//   with an additional tick-offset inside the slice itself,
			//   (which, depending on whether the sequence is playing,
			//   either takes the sequence's current tick, or the global absolute tick,
			//   and wraps said tick to the size of a single chunk)
			POS[seq] = (csize * nums) + (((STATS[seq] & 128) ? POS[seq] : CUR16) % csize);

			if (!(STATS[seq] & 128)) { // If the seq was previously turned off...
				TO_UPDATE |= 4 << row; // Flag the seq's corresponding LED-row for an update
			}

			STATS[seq] |= 128; // Set the sequence to playing

		}
	}

}

// Parse a note-keypress that was generated while in RECORDMODE mode
void parseRecPress(byte col, byte row) {

	byte key = col + (row * 4); // Get the button-key that corresponds to the given column and row
	byte ctrl = BUTTONS & B00111111; // Get the control-row buttons' activity

	if ((!ctrl) || (ctrl & B00100000)) { // If no CTRL buttons are held, or a RECORDING-based chord is held...

		// Get the interval-buttons' activity specifically,
		// while weeding out any false-positives from commands that don't hold the INTERVAL button (B00000001)
		byte ibs = (ctrl & 7) * ((ctrl <= 7) && (ctrl & 1));

		// Get a note that corresponds to the key, organized from bottom-left to top-right, with all modifiers applied
		byte pitch = (OCTAVE * 12) + BASENOTE + ((23 - key) ^ 3);
		while (pitch > 127) { // If the pitch is above 127, which is the limit for MIDI notes...
			pitch -= 12; // Reduce it by 12 until it is at or below 127
		}

		// Get the note's velocity, with a random humanize-offset
		byte velo = VELO - min(VELO - 1, byte(GLOBALRAND % (HUMANIZE + 1)));

		byte rchan = CHAN & 15; // Strip the chan of any special-command flag bits

		if (ibs) { // If any INTERVAL-keys are held...
			// Make a composite INTERVAL command, as it would appear in data-storage,
			// and apply the INTERVAL command to the channel's most-recent pitch
			ibs = ((ibs & 1) << 7) | ((ibs & 2) << 5) | ((ibs & 4) << 3);
			pitch = applyIntervalCommand(ibs | key, RECENT[rchan]);
		}

		if (ibs || (rchan == CHAN)) { // If INTERVAL keys are held, or this is a normal NOTE...
			RECENT[rchan] = pitch; // Update the channel's most-recent note to the pitch-value
		}

		if (PLAYING && (ctrl & B00100000)) { // If notes are being recorded into a playing sequence...
			// Record the note, either natural or modified by INTERVAL, into the current RECORDSEQ slot;
			// and if this is an INTERVAL-command, then clip off any virtual CC-info from the chan-byte 
			recordToSeq(
				POS[RECORDSEQ] - (POS[RECORDSEQ] % QUANTIZE), // Get a rounded-down insert-point, based on QUANTIZE
				ibs ? (CHAN % 16) : CHAN,
				pitch,
				velo
			);
			// Incerase the record-position by QUANTIZE amount, wrapping around the end of the sequence if applicable
			TO_UPDATE |= 254; // Flag the 7 bottommost LED-rows for an update
		}

		if (CHAN & 16) { return; } // If this was a virtual CC command, forego all SUSTAIN operations

		// Update SUSTAIN buffer in preparation for playing the user-pressed note
		clipBottomSustain(); // If the bottommost SUSTAIN is filled, send its NOTE-OFF prematurely
		memmove(SUST + 3, SUST, SUST_COUNT * 3); // Move all sustains one space downward
		SUST[0] = 128 + CHAN; // Fill the topmost sustain-slot with the user-pressed note
		SUST[1] = pitch; // ^
		SUST[2] = DURATION; // ^
		SUST_COUNT++; // Increase the number of active sustains, to accurately reflect the new note
		byte buf[4] = {byte(144 + CHAN), pitch, velo}; // Build a correctly-formatted NOTE-ON for the user-pressed note
		Serial.write(buf, 3); // Send the user-pressed note to MIDI-OUT immediately, without buffering

		TO_UPDATE |= 2; // Flag the sustain-row for a GUI update

	} else { // Else, if CTRL-buttons unrelated to RECORDING are held...

		// Get the button's var-change value, for when a global var is being changed
		char change = (32 >> row) * ((col & 2) - 1);

		if (ctrl == B00010000) { // If the BASENOTE button is held...
			BASENOTE = clamp(0, 11, char(BASENOTE) + change); // Modify the BASENOTE value
			TO_UPDATE |= 1; // Flag the topmost row for updating
		} else if (ctrl == B00001000) { // If the OCTAVE button is held...
			OCTAVE = clamp(0, 10, char(OCTAVE) + change); // Modify the OCTAVE value
			TO_UPDATE |= 1; // Flag the topmost row for updating
		} else if (ctrl == B00000100) { // If the DURATION button is held...
			DURATION = clamp(0, 255, int(DURATION) + change); // Modify the DURATION value
			TO_UPDATE |= 1; // Flag the topmost row for updating
		} else if (ctrl == B00000010) { // If the CHANNEL button is held...
			CHAN = (CHAN & 16) | clamp(0, 15, char(CHAN & 15) + change); // Modify the CHAN value, while preserving CC flag
			TO_UPDATE |= 1; // Flag the topmost row for updating
		} else if (ctrl == B00110000) { // If the SWITCH RECORDING SEQUENCE button is held...
			TO_UPDATE |= 4 >> (RECORDSEQ >> 2); // Flag the previous seq's corresponding LED-row for updating
			resetSeq(RECORDSEQ); // Reset the current record-seq, which is about to become inactive
			RECORDSEQ = (PAGE * 24) + key; // Switch to the seq that corresponds to the key-position on the active page
			primeRecSeq(); // Prime the newly-entered RECORD-MODE sequence for editing
			TO_UPDATE |= 4 >> row; // Flag the new seq's corresponding LED-row for updating
			TO_UPDATE |= 1; // Flag the topmost row for updating
		} else if (ctrl == B00101000) { // If the CLOCK-MASTER command is held...
			CLOCKMASTER ^= 1; // Toggle the CLOCK-MASTER value
			ABSOLUTETIME = micros(); // Set the ABSOLUTETIME-tracking var to now
			ELAPSED = 0; // Set the ELAPSED value to show that no time has elapsed since the last tick-check
			TO_UPDATE |= 1; // Flag the topmost row for updating
		} else if (ctrl == B00100100) { // If the QUANTIZE button is held...
			QUANTIZE = clamp(0, 16, abs(change)); // Modify the QUANTIZE value
			TO_UPDATE |= 1; // Flag the topmost row for updating
		} else if (ctrl == B00100010) { // If the PROGRAM-CHANGE command is held...

			// todo: add PROGRAM-CHANGE functionality

		} else if (ctrl == B00100001) { // If the CONTROL-CHANGE command is held...
			CHAN ^= 16; // Flip the CHAN bit that turns all NOTE command-entry into CC command-entry
			TO_UPDATE |= 1; // Flag the topmost row for updating
		} else if (ctrl == B00011000) { // If the VELO button is held...
			VELO = clamp(0, 127, int(VELO) + change); // Modify the VELO value
			TO_UPDATE |= 1; // Flag the topmost row for updating
		} else if (ctrl == B00010100) { // If the HUMANIZE button is held...
			HUMANIZE = clamp(0, 127, int(HUMANIZE) + change); // Modify the HUMANIZE value
			TO_UPDATE |= 1; // Flag the topmost row for updating
		} else if (ctrl == B00010010) { // If the SEQ-SIZE button is held...
			// Get the new value for the currently-recording-seq's size
			byte newsize = byte(clamp(0, 63, char(STATS[RECORDSEQ] & 63) + change));
			STATS[RECORDSEQ] = (STATS[RECORDSEQ] & 128) | newsize; // Modify the currently-recording seq's size
			updateFileByte(RECORDSEQ + 1, STATS[RECORDSEQ]); // Update the seq's size-byte in the song's savefile
			POS[RECORDSEQ] %= newsize << 4; // Wrap around the currently-recording seq's 16th-note-position
			TO_UPDATE = 255; // Flag all LED-rows for updating
		} else if (ctrl == B00010001) { // If the BPM command is held...
			BPM = clamp(16, 200, int(BPM) + change); // Change the BPM rate
			updateFileByte(0, BPM); // Update the BPM-byte in the song's savefile
			updateTickSize(); // Update the internal tick-size (in microseconds) to match the new BPM value
			TO_UPDATE |= 1; // Flag the topmost row for updating
		}

	}

}

// Interpret an incoming keystroke, using a given button's row and column
void assignKey(byte col, byte row) {

	byte ctrl = BUTTONS & B00111111; // Get the control-row buttons' activity

	//byte key = col + (row * 4); // Get the button-key that corresponds to the given column and row

	if (col == 0) { // If the keystroke is in the leftmost column...

		newGestureEntry(row); // Create a new entry in the GESTURE-tracking system for this keypress
		checkForGestures(); // Check whether any gestures have just been completed

		if (RECORDMODE) { // If RECORD-MODE is active...
			TO_UPDATE |= 253; // Flag the top LED-row, and bottom 6 LED-rows, for updating
		}

	} else { // Else, if the keystroke is in any of the other columns...

		if (LOADMODE) { // If LOAD-MODE is active...
			RECORDMODE = 0; // Toggle out of RECORD-MODE to prevent timing-mismatch bugs
			// Load a song whose file corresponds to the button that was pressed,
			// on a page that corresponds to the highest control-button that is being held
			loadSong((col - 1) + (row * 4) + (24 * ctrlToButtonIndex(ctrl)));
			LOADMODE = 0; // Exit LOAD-MODE automatically
		} else if (RECORDMODE) { // If RECORD-MODE is active...
			parseRecPress(col - 1, row); // Parse the RECORD-MODE button-press
		} else { // Else, if PLAY MODE is active...
			parsePlayPress(col - 1, row); // Parse the PLAY-MODE button-press
		}

	}

}

// Interpret a key-release according to whatever command-mode is active
void unassignKey(byte col) {
	if (col == 0) { // If this up-keystroke was in the leftmost column...
		if (RECORDMODE) { // If RECORD-MODE is active...
			TO_UPDATE |= 253; // Flag the top LED-row, and bottom 6 LED-rows, for updating
		}
	}
}
