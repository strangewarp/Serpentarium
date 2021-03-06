

// Move the AUTOCURSOR, which controls the position of all AUTOCMD operations for both ON LOAD and ON EXIT commands
void acCursorCmd(byte col, byte row) {
  AUTOCURSOR = (AUTOCURSOR + byte(32 + toChange(col, row))) % 16; // Modify the AUTOCURSOR value, wrapping it within the valid range
	TO_UPDATE |= 253; // Flag the topmost LED-row, and bottom 6 LED-rows, for updating
}

// Delete the special-command at the AUTOCURSOR's current position,
//   in either the ON LOAD or ON EXIT block depending on whether REPEAT is toggled
void acDeleteCmd(__attribute__((unused)) byte col, __attribute__((unused)) byte row) {

  // Get the position of the special-command that currently corresponds to the AUTOCURSOR,
  //   for either ON LOAD or ON EXIT, depending on whether REPEAT is toggled
  byte pos = (AUTOCURSOR * 3) + (REPEAT ? FILE_ONEXIT_START : FILE_ONLOAD_START);

  for (byte i = 0; i < 3; i++) { // For every byte in the special-command...
    updateNonMatch(pos + i, 0); // Clear that command's contents in the savefile
  }

	TO_UPDATE |= 252; // Flag the bottom 6 rows for LED updates
  
}

// Insert a special-command at the AUTOCURSOR's current position,
//   in either the ON LOAD or ON EXIT block depending on whether REPEAT is toggled,
//   with the command itself made out of the CHAN byte, the PITCH byte, and then the VELOCITY byte (not modified by any humanize).
void acInsertCmd(byte col, byte row) {

  if (CHAN < 128) { return; } // If the current CHAN command is a Tine-only internal command, don't put it into the AUTOCMD blocks

  // Get the position of the special-command that currently corresponds to the AUTOCURSOR,
  //   for either ON LOAD or ON EXIT, depending on whether REPEAT is toggled
  byte pos = (AUTOCURSOR * 3) + (REPEAT ? FILE_ONEXIT_START : FILE_ONLOAD_START);

  byte pitch = modKeyPitch(col, row); // Get the keystroke's corresponding "pitch-byte" value
  
  // Insert a new special-command into the savefile, at the given AUTOCURSOR position, in either the ON LOAD or ON EXIT block
  updateNonMatch(pos, CHAN);
  updateNonMatch(pos + 1, pitch);
  updateNonMatch(pos + 2, VELO);

  // Create and send a MIDI-command that corresponds to what has just been inserted into the AUTOCMD header-block
  byte buf[4] = {CHAN, pitch, VELO, 0};
  Serial.write(buf, 2 + ((CHAN % 224) <= 191));
  
	TO_UPDATE |= 252; // Flag the bottom 6 rows for LED updates
  
}

// Toggle whether a sequence will automatically start playing on FILE-LOAD
void actOnLoadCmd(__attribute__((unused)) byte col, __attribute__((unused)) byte row) {
  STATS[RECORDSEQ] ^= 64; // Flip the current RECORDSEQ's "ACTIVE ON LOAD" bit within its STATS byte
	TO_UPDATE |= 1; // Flag the topmost row for updating
}

// Arm or disarm RECORDNOTES
void armCmd(__attribute__((unused)) byte col, __attribute__((unused)) byte row) {
	RECORDNOTES ^= 1; // Arm or disarm the RECORDNOTES flag
	TO_UPDATE |= 252; // Flag the bottom 6 rows for LED updates
}

// Parse an ARPEGGIATOR MODE press
void arpModeCmd(__attribute__((unused)) byte col, __attribute__((unused)) byte row) {
	ARPMODE = (ARPMODE + 1) % 3; // Toggle the ARPEGGIATOR MODE
	TO_UPDATE |= 1; // Flag the topmost row for updating
}

// Parse an ARPEGGIATOR REFRESH press
void arpRefCmd(__attribute__((unused)) byte col, __attribute__((unused)) byte row) {
	ARPREFRESH ^= 1; // Toggle the ARPEGGIATOR REFRESH behavior (whether or not RPTVELO is refreshed on every new keypress in REPEAT-mode)
	TO_UPDATE |= 1; // Flag the topmost row for updating
}

// Parse a CHAIN DIRECTION press
void chainCmd(byte col, byte row) {
	byte cmd = pgm_read_byte_near(CHAIN_MATRIX + (row * 4) + col); // Get the CHAIN command that corresponds to the given button
	if (cmd == 255) { // If this is a CLEAR command...
		CHAIN[RECORDSEQ] = 0; // Empty out the seq's CHAIN data
	} else { // Else, if this is a CHAIN command...
		CHAIN[RECORDSEQ] ^= 1 << cmd; // Toggle the bit that corresponds to a given CHAIN-direction, in the seq's CHAIN-entry
	}
	TO_UPDATE |= 252; // Flag the bottom 6 rows for LED updates
}

// Parse a CHAN press
void chanCmd(byte col, byte row) {
	// Modify the CHAN value, keeping it within the range of valid/supported commands
	CHAN = min(239, (CHAN & 240) | applyChange(CHAN & 15, toChange(col, row), 0, 15));
	TO_UPDATE |= 1; // Flag the topmost row for updating
}

// Parse a CLEAR NOTES press:
// Clear a given number of beats' worth of notes, from the currently-active track in the current RECORDSEQ.
void clearCmd(byte col, byte row) {

	byte buf[5]; // Create a note-sized data buffer...
	memset(buf, 0, 4); // ...And clear it of any junk-data

	byte len = (STATS[RECORDSEQ] & 31) + 1; // Get the RECORDSEQ's length, in whole-notes
	word blen = word(len) * 128; // Get the RECORDSEQ's length, in bytes

	unsigned long rspos = FILE_BODY_START + (FILE_SEQ_BYTES * RECORDSEQ); // Get the RECORDSEQ's absolute data-position
	word pos = POS[RECORDSEQ] & 2040; // Get the bottom-point of the current whole-note in RECORDSEQ
	byte t4 = TRACK * 4; // Get a bitwise offset based on whether track 1 or 2 is active

	for (word i = 0; i < (word(min(abs(toChange(col, row)), len)) * 256); i += 8) { // For every whole-note in the clear-area...
		writeCommands(rspos + ((pos + i + t4) % blen), 4, buf, 1); // Overwrite it if it matches the TRACK and CHAN
	}

	file.sync(); // Sync any still-buffered data to the savefile

	setBlink(TRACK, 0, 0, 0); // Start a TRACK-linked LED-BLINK

}

// Parse a DURATION press
void durationCmd(byte col, byte row) {
	simpleChange(col, row, DURATION, 0, 129, 1);
	if (DURATION == 129) { // If DURATION has just been toggled into MANUAL-MODE...
		REPEAT = 0; // Clear REPEAT, which is incompatible with MANUAL-MODE
	}
}

// Parse a DURATION-HUMANIZE press
void durHumanizeCmd(byte col, byte row) {
	simpleChange(col, row, DURHUMANIZE, 0, 128, 1);
}

// Parse all of the possible actions that signal the recording of commands
void genericCmd(byte col, byte row) {

	RECPRESS = 1; // Set the "a note is currently being pressed" flag
	RECNOTE = modKeyPitch(col, row); // Save the col and row's corresponding pitch, as the latest note pressed in RECORD-MODE

	arpPress(); // Parse a new keypress in the arpeggiation-system

	if (REPEAT) { // If REPEAT is active...
		if ((!ARPLATCH) || ARPREFRESH) { // If ARPLATCH isn't active, or if ARPREFRESH is active, then RPTVELO needs to be refreshed. So...
			RPTVELO = VELO; // Refresh the REPEAT-VELOCITY tracking var to be equal to the user-defined VELOCITY amount
		}
		return; // Exit the function, since no commands should be sent instantly
	} else { // Else, if REPEAT isn't active...
		if (DURATION == 129) { // If DURATION is in manual-mode...
			if (KEYFLAG) { // If another key is already held in manual-mode...
				recordHeldNote(); // Record that held key's note
			}
			setKeyNote(col, row); // Set a held-recording-note for the given button-position
		} else { // Else, if DURATION is in auto-mode...
			processRecAction(RECNOTE); // Parse the key as a recording-action into the current TRACK, and apply OFFSET
		}
	}

	TO_UPDATE |= 252; // Flag the bottom 6 LED-rows for an update

}

// Parse a GRID-CONFIG press
void gridConfigCmd(byte col, byte row) {
	simpleChange(col, row, GRIDCONFIG, 0, GRID_TOTAL, 1);
}

// Parse a HUMANIZE press
void humanizeCmd(byte col, byte row) {
	simpleChange(col, row, HUMANIZE, 0, 127, 1);
}

// Parse an OCTAVE press
void octaveCmd(byte col, byte row) {
	simpleChange(col, row, OCTAVE, 0, 10, 1);
}

// Parse an OFFSET press
void offsetCmd(byte col, byte row) {
	char change = toChange(col, row); // Convert a column and row into a CHANGE value
	OFFSET = min(31, max(-31, OFFSET + change)); // Modify the OFFSET value
	TO_UPDATE |= 1; // Flag the topmost row for updating
}

// Parse a SHIFT POSITION press
void posCmd(byte col, byte row) {

	int change = int(toChange(col, row)) * 32; // Convert a column and row into a CHANGE value, in 32nd-notes

	CUR32 = byte(word(((int(CUR32) + change) % 256) + 256) % 256); // Shift the global cue-point, wrapping in either direction

	if (RECORDMODE) { // If RECORDMODE is active... (This check is necessary because posCmd() is called from both RECORD MODE and PLAY MODE)
		for (byte seq = 0; seq < 48; seq++) { // For each seq...
			if (STATS[seq] & 128) { // If the seq is playing...
				word size = seqLen(seq); // Get the seq's size, in 32nd-notes
				POS[seq] = word(((long(POS[seq]) + change) % size) + size) % size; // Shift the seq's position, wrapping in either direction
			}
		}
		TO_UPDATE |= 2; // Flag the SEQ row (in RECORD MODE) for LED updates
	}

	TO_UPDATE |= 1; // Flag the GLOBAL-CUE row for LED updates

}

// Parse a QRESET press
void qrstCmd(byte col, byte row) {
	simpleChange(col, row, QRESET, 0, 128, 1);
}

// Parse a QUANTIZE press
void quantizeCmd(byte col, byte row) {
	simpleChange(col, row, QUANTIZE, 1, 32, 1);
}

// Parse a REPEAT press
void repeatCmd(__attribute__((unused)) byte col, __attribute__((unused)) byte row) {
	if (DURATION == 129) { // If DURATION is in MANUAL-MODE...
		DURATION = 1; // Remove DURATION from MANUAL-MODE, since this would be incompatible with REPEAT
	}
	REPEAT ^= 1; // Arm or disarm the NOTE-REPEAT flag
	TO_UPDATE |= 252; // Flag the bottom 6 rows for LED updates
}

// Parse a REPEAT-SWEEP press
void rSweepCmd(byte col, byte row) {
	simpleChange(col, row, RPTSWEEP, 0, 255, 253);
}

// Parse a SEQ-SIZE press
void sizeCmd(byte col, byte row) {
	char change = toChange(col, row); // Convert a column and row into a CHANGE value
	// Get the new value for the currently-recording-seq's size, and modify the seq's STATS byte with it
	STATS[RECORDSEQ] = (STATS[RECORDSEQ] & 224) | applyChange(STATS[RECORDSEQ] & 31, change, 0, 31);
	resetAllTiming(); // Reset the timing of all seqs and the global cue-point
	TO_UPDATE |= 3; // Flag top two rows for updating
}

// Parse a SWITCH RECORDSEQ press
void switchCmd(byte col, byte row) {
	updateSeqHeader(); // Update the current seq's header-data-byte in the savefile, if applicable
	updateSavedChain(); // Update the current seq's CHAIN-byte in the savefile, if applicable
	TO_UPDATE |= 3 | (4 >> (RECORDSEQ >> 2)); // Flag the old seq's LED-row for updating, plus the top two rows
	resetSeq(RECORDSEQ); // Reset the current record-seq, which is about to become inactive
	RECORDSEQ = (PAGE * 24) + col + (row * 4); // Switch to the seq that corresponds to the key-position on the active page
	resetAllTiming(); // Reset the timing of all seqs and the global cue-point
}

// Parse a BPM-press
void tempoCmd(byte col, byte row) {
	char change = toChange(col, row); // Convert a column and row into a CHANGE value
	BPM = applyChange(BPM, change, BPM_LIMIT_LOW, BPM_LIMIT_HIGH); // Change the BPM rate
	updateTickSize(); // Update the internal tick-size (in microseconds) to match the new BPM value
	TO_UPDATE |= 1; // Flag the topmost row for updating
}

// Parse a TRACK-press
void trackCmd(__attribute__((unused)) byte col, __attribute__((unused)) byte row) {
	TRACK = !TRACK; // Toggle between tracks 1 and 2
	TO_UPDATE |= 1; // Flag the topmost row for updating
}

// Parse an UPPER COMMAND BITS press
void upperBitsCmd(byte col, byte row) {
	// Convert col/row into a CHANGE val, bounded to +/- 16, 32, 64
	char change = max(-8, min(8, toChange(col, row))) << 4;
	// Apply that change-interval to the CHAN value, bounded to valid commands
	CHAN = (CHAN & 15) | applyChange(CHAN & 240, change, UPPER_BITS_LOW, UPPER_BITS_HIGH);
	TO_UPDATE |= 1; // Flag the topmost row for updating
}

// Parse a VELOCITY press
void veloCmd(byte col, byte row) {
	simpleChange(col, row, VELO, 0, 127, 1);
}
