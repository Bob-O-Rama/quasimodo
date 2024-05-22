# quasimodo
Schulmerich Gen 4 DSP Carillon Tools &amp; Info

quasimodo is a C/C++ based tool to parse and list Schulmerich AutoBel Card images.   AutoBel Cards are PCMCIA linear flash cards, usually 1MB in size used by Schulmerich Gen 4 Electronic Carillon systems to store and transfer a proprietary MIDI format structured into collections and songs.   Each card can store up to 32 collections, and each collection consists of a dozen songs.   The Gen 4 systems can also be used by the carillon operator to record new songs by playing them on the systems ( piano style ) keyboard.  This built in encoder creates custom collections of user songs whioch can be played or scheduled like selections that were provided on AutoBel cards.

Verdin bought Schulmeric around 2008, and has not retained or made available technical information about these systems, the data structure of the cards, or program source or binaries.   All information provided by and used in this project was the result of observing system behaviour during operations and was not derived from information provided by Verdin.

None of it is guranteed to be accurate, nor does the presence of any details constitute an assertion of intellectual prioperty rights or ownership thereof.  A statement I recieved from Verdin indicates they assert no proprietary rights over the "voices or music produced by or provided with" the Schulmeric products.  However in the interest of reducing the chances of infringing on their rights, the card image files provided are my own recorded songs. 

quasimodo.cpp is a quick hack quality source with several functions:

1. Read and parse ( Schulmeric provided ) AutoBel Card and ( user created ) Encode Backup Card
2. Provide listings of card contents: collections and songs, and their technical details
3. Emulate the behaviour of the AutoBel Player built into the carillon by sequencing the proprietary MIDI-like data for a song and create TTY output needed to play the selected songs
4. Maintain a list of MIDI "patches" ( custom sequences of MIDI instructions used for setup and control purposes ) and a means to specify lists of patch sequences to replay before, during, and after a song is sequences.
5. Patch sequences can be sent to the carillon even in the absence of an AutoBel Card / Encode Backup Card.
6. Patch sequences can be as long as a song on card, and can therefor be used as an alternative format for storing arbitrary MIDI data to be sequenced
7. Patch sequences can include a variable which can be specified at the command line, which provides a list / range of values to try in the patch.   This allows for "noodling around" with different values to see what they do.
8. The tempo / playback speed of the sequencer can be set arbitrarily fast or slow

The code is functional, works, but arguably a horrible source - if you think you can make things better, become a collaborator and make it awesome. 
