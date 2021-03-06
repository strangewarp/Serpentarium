

// MIDI PANIC: Send "ALL NOTES OFF" CC-commands on every MIDI channel.
void midiPanic() {

	byte note[4] = {176, 123, 0, 0}; // Create a generic "ALL NOTES OFF" CC command

	for (; note[0] <= 15; note[0]++) { // For every MIDI channel...
		Serial.write(note, 3); // Send the ALL-NOTES-OFF CC command to MIDI-OUT immediately
	}

}

// Parse a given MIDI command
void parseMidiCommand(byte &rcd) {

	byte didplay = 0; // Track whether a command has been sent by this function or not

	if (RECORDMODE) { // If RECORD-MODE is active...
		byte cmd = INBYTES[0] & 240; // Get the command-type
		byte chn = INBYTES[0] & 15; // Get the MIDI channel
		if ((BUTTONS & B00111111) == B00100000) { // If notes are currently being recorded...
			if ((CHAN & 15) == chn) { // If this command is within the currently-selected CHANNEL...
				// If REPEAT is inactive, DURATION is in manual-mode, and the command is either a NOTE-ON or NOTE-OFF...
				if ((!REPEAT) && (DURATION == 129) && (cmd <= 144)) {
					if (KEYFLAG) { // If a note is currently being pressed...
						recordHeldNote(); // Record the held note and reset its associated tracking-vars
						rcd++; // Flag that at least one command has been recorded on this tick
					}
					if (cmd == 144) { // If this command is a NOTE-ON...
						setRawKeyNote(INBYTES[1], INBYTES[2]); // Key-tracking mechanism: start tracking the unmodified held-note
					}
					didplay = 1; // Flag that a note has already been played by this function
				} else { // Else, if REPEAT is active, or DURATION is in auto-mode...
					if ((cmd >= 144) && (cmd <= 239)) { // If this is a valid command for auto-mode recording...
						// Record the incoming MIDI command, clamping DURATION for situations where REPEAT=1,DURATION=129
						recordToSeq(POS[RECORDSEQ], min(128, DURATION), chn, INBYTES[1], INBYTES[2]);
						rcd++; // Flag that at least one command has been recorded on this tick
					}
				}
			}
		}
	}

	if (didplay) { return; } // If a command was already sent by this function, exit the function

	Serial.write(INBYTES, INTARGET); // Having parsed the command, send its bytes onward to MIDI-OUT

}

// Parse all incoming raw MIDI bytes
void parseRawMidi() {

	byte recced = 0; // Flag that tracks whether any commands have been recorded

	// While new MIDI bytes are available to read from the MIDI-IN port...
	while (Serial.available() > 0) {

		byte b = Serial.read(); // Get the frontmost incoming byte

		if (SYSIGNORE) { // If this is an ignoreable SYSEX command...
			if (b == 247) { // If this was an END SYSEX byte, clear SYSIGNORE and stop ignoring new bytes
				SYSIGNORE = 0;
			}
		} else { // Else, accumulate arbitrary commands...

			if (INCOUNT >= 1) { // If a command's first byte has already been received...
				INBYTES[INCOUNT] = b;
				INCOUNT++;
				if (INCOUNT == INTARGET) { // If all the command's bytes have been received...
					parseMidiCommand(recced); // Parse the command
					memset(INBYTES, 0, 3); // Reset incoming-command-tracking vars
					INCOUNT = 0;
				}
			} else { // Else, if this is either a single-byte command, or a multi-byte command's first byte...

				byte cmd = b & 240; // Get the command-type of any given non-SYSEX command

				if (
					(b == 244) // MISC SYSEX command
					|| (b == 240) // START SYSEX MESSAGE command
				) { // Anticipate an incoming SYSEX message
					SYSIGNORE = 1;
				} else if (
					(b >= 240)      // Any special-command
					|| (cmd == 224) // PITCH BEND command
					|| (cmd == 176) // CONTROL CHANGE command
					|| (cmd == 160) // POLY-KEY PRESSURE command
					|| (cmd == 144) // NOTE-ON command
					|| (cmd == 128) // NOTE-OFF command
				) { // Anticipate an incoming 3-byte message
					INBYTES[0] = b;
					INCOUNT = 1;
					INTARGET = 3;
				} else if (
					(b == 243) // SONG SELECT command
					|| (b == 241) // MIDI TIME CODE QUARTER FRAME command
					|| (cmd == 208) // CHAN PRESSURE (AFTERTOUCH) command
					|| (cmd == 192) // PROGRAM CHANGE command
				) { // Anticipate an incoming 2-byte message
					INBYTES[0] = b;
					INCOUNT = 1;
					INTARGET = 2;
				}

			}
		}

	}

	if (recced) { // If any incoming commands have been recorded on this tick...
		file.sync(); // Sync the data in the savefile
	}

}
