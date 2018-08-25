

// MIDI PANIC: Send a NOTE-OFF to every note on every MIDI channel.
//     This is done twice over, for maximum thoroughness:
//     Firstly, as a series of "ALL NOTES OFF" CC-commands on every MIDI channel;
//     Secondly, as a series of manual "NOTE-OFF" commands for every pitch on every MIDI channel.
void midiPanic() {

	byte note[4]; // Create a note-buffer to hold a single note's worth of data at a time

	note[1] = 123; // Set byte 2 to the "ALL NOTES OFF" CC command
	note[2] = 0; // As per MIDI spec, set byte 3 to "0" (empty)
	for (byte i = 0; i <= 15; i++) { // For every MIDI channel...
		note[0] = 176 + i; // Create a CC command-byte for the current channel
		Serial.write(note, 3); // Send the ALL-NOTES-OFF CC command to MIDI-OUT immediately
	}

	note[2] = 127; // Set the velocity-byte to 127, for the sake of completeness

	for (byte i = 0; i <= 15; i++) { // For every MIDI channel...
		sendRow(0, i); // Send a visible status to the top LED-row
		note[0] = 128 + i; // Create a NOTE-OFF command for the current channel
		for (byte j = 0; j <= 127; j++) { // For every pitch...
			note[1] = j; // Put that pitch into the note-buffer
			Serial.write(note, 3); // Send the NOTE-OFF command to MIDI-OUT immediately
		}
	}

	sendRow(0, 0); // Empty the command's LED-status-row before finishing

}

// Parse a given MIDI command
void parseMidiCommand(byte &rcd) {
	if (RECORDMODE) { // If RECORD-MODE is active...
		byte cmd = INBYTES[0] & 240; // Get the command-type
		byte chn = INBYTES[0] & 15; // Get the MIDI channel
		if ((BUTTONS & B00111111) == B00100000) { // If notes are currently being recorded...
			if ((CHAN & 15) == chn) { // If this command is within the currently-selected CHANNEL...
				if ((cmd >= 144) && (cmd <= 239)) { // If this is a valid command...
					recordToSeq(POS[RECORDSEQ], chn, INBYTES[1], INBYTES[2], TRACK % 2); // Record the incoming MIDI command
					rcd |= 1; // Flag that at least one command has been recorded on this tick
				}
			}
		}
	}
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
				SYSIGNORE = false;
			}
		} else { // Else, accumulate arbitrary commands...

			if (INCOUNT >= 1) { // If a command's first byte has already been received...
				INBYTES[INCOUNT] = b;
				INCOUNT++;
				if (INCOUNT == INTARGET) { // If all the command's bytes have been received...
					parseMidiCommand(recced); // Parse the command
					memset(INBYTES, 0, sizeof(INBYTES) - 1); // Reset incoming-command-tracking vars
					INCOUNT = 0;
				}
			} else { // Else, if this is either a single-byte command, or a multi-byte command's first byte...

				byte cmd = b & 240; // Get the command-type of any given non-SYSEX command

				if (b == 248) { // TIMING CLOCK command
					Serial.write(b); // Send the byte to MIDI-OUT
					if (!CLOCKMASTER) { // If the clock is in MIDI FOLLOW mode...
						CUR16++; // Since we're sure we're on a new 16th-note, increase the current-16th-note variable
						if (!(CUR16 % 16)) { // If the global 16th-note is the first within a global slice...
							TO_UPDATE |= 1; // Flag the global-cue row of LEDs for an update
						}
						iterateAll(); // Iterate through one step of all sequencing processes
					}
				} else if (b == 250) { // START command
					Serial.write(b); // Send the byte to MIDI-OUT
					if (!CLOCKMASTER) { // If the clock is in MIDI FOLLOW mode...
						resetAllSeqs(); // Reset sequences to their initial positions
						toggleMidiClock(false); // Toggle the MIDI clock, with an "external command" flag
					}
				} else if (b == 251) { // CONTINUE command
					Serial.write(b); // Send the byte to MIDI-OUT
					if (!CLOCKMASTER) { // If the clock is in MIDI FOLLOW mode...
						toggleMidiClock(false); // Toggle the MIDI clock, with an "external command" flag
					}
				} else if (b == 252) { // STOP command
					Serial.write(b); // Send the byte to MIDI-OUT
					if (!CLOCKMASTER) { // If the clock is in MIDI FOLLOW mode...
						haltAllSustains(); // Halt all currently-sustained notes
						toggleMidiClock(false); // Toggle the MIDI clock, with an "external command" flag
					}
				} else if (b == 247) { // END SYSEX MESSAGE command
					// If you're dealing with an END-SYSEX command while SYSIGNORE is inactive,
					// then that implies you've received either an incomplete or corrupt SYSEX message,
					// and so nothing should be done here.
				} else if (
					(b == 244) // MISC SYSEX command
					|| (b == 240) // START SYSEX MESSAGE command
				) { // Anticipate an incoming SYSEX message
					SYSIGNORE = true;
				} else if (
					(b == 242) // SONG POSITION POINTER command
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

