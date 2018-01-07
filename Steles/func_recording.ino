
// Record a given MIDI command into the tempdata-file of the current RECORDSEQ sequence
void recordToSeq(char offset, byte chan, byte b1, byte b2) {

	byte buf[9] = {0, 0, 0, 0, 0, 0, 0, 0}; // SD-card read/write buffer

	// Create a virtual tick-position that compensates for any given tick-offset
	word tick = word((int(POS[RECORDSEQ]) + offset) % (word(STATS[RECORDSEQ] & 63) << 4));

	// Get the position of the first of this tick's bytes in the data-file
	unsigned long tickstart = (49UL + (8192UL * RECORDSEQ)) + (tick << 3);

	// Get the tick's note-slots, and check whether any of them are empty
	file.seekSet(tickstart);
	file.read(buf, 8);

	// If none of the note-slots are empty, then shift their contents downward by one note, deleting the lowest item
	if (buf[7]) {
		file.seekSet(tickstart + 4);
		file.write(buf, 4);
	}

	// Set the insert-point to the highest unfilled note in the tick,
	// or to the first note in the tick, if the contents were shifted downward
	file.seekSet(tickstart + (((!!buf[3]) ^ (!!buf[7])) << 2));

	// Construct a virtual MIDI command, with an additional DURATION value, in the write buffer
	buf[0] = (chan <= 15) * DURATION; // Only include the DURATION if this isn't a flagged special-command
	buf[1] = chan;
	buf[2] = b1;
	buf[3] = b2;

	file.write(buf, 4); // Write the note to the current tempdata

	file.sync(); // Force this function's write processes to be committed to the savefile immediately

}
