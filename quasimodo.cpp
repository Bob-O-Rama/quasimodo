/*
    quasimodo - a command line tool for Schulmerich Generation 4 DSP based electronic carillons
                A quick hack to read, parse, and extract data from AutoBel Card images.
*/
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <iterator>
#include <regex>
#include <vector>
#include <cmath>
#include <cstring>
#include <string>
#include <ctime>
#include <random>
#include <unistd.h>

// For Serial I/O Mess

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
// Linux-specific
#include <linux/serial.h>
#include <linux/ioctl.h>
#include <asm/ioctls.h>


int   loglevel      = 0;       // No logging, just shut up already.
bool  debug         = false;   // No debugging
bool  showhelp      = false;   // show detailed help
float tempo         = 24;      // Number of delay byte counts / second
float transpose     = 0;       // value to add to MIDI note numbers
float bps           = 38400;   // baud rate for playback
bool  play          = false;   // Play the specified ( card, collection, song )
bool  listcolls     = false;   // list collections
bool  listsongs     = false;   // list songs
bool  tty           = false;   // Send MIDI out TTY port
bool  delimited     = true;    // 
int   delimiter     = 0xFF;    //
int   collection_id = -1;      // the specified collection ID used for playback / extract
int   song_id       = -1;      // the specified song ID used for playback / extract
bool  cardloaded    = false;   // Set to true if the buffer contains a valis AutoBel Card image.
int   cardsize      = 0;       // size of data in cardimage
std::string cardbuffer ;       // buffer for holding the card image
bool  patchload     = true;    // Load patch table into memory
bool  patchtest     = false;   // Loop through various patches to discover stuff
std::string inFile  = "";
std::string outFile = "";
std::string ttyFile = "/dev/ttyUSB0";
std::string patchFile = "./quasimodo.patches";
std::vector<std::string> patchdata ; // raw patch strings from the patch table file
std::vector<std::string> patchname ; // name of the patch
std::vector<std::string> patchin;    // series of patch names from command line for leadin...
std::vector<std::string> patches;    // playing...
std::vector<std::string> patchout;   // leadout...
std::vector<int> tryvalue; // A set of series of values to substitute for a variable
int currenttry;            // The current value to substitute for variables in patches
struct termios oldtio, newtio;
struct serial_struct ser_info;
int serial; // Awaken the old gods


using namespace std;

#define OPTIONBOOL(O,V,X) \
       if ( ! parsed && j < argc && 0 == strcmp(argv[j], #O) ) \
       { \
           (V) = (X); \
           if( loglevel > 6 ) cout << "Parsed argv[" << j << "]='" << argv[j] \
                            << "' option ( " << #O << " ).  Setting " << #V << " to " << (V) << std::endl; \
           for(int i=1; i<argc; ++i) \
               argv[i]  = argv[i+1]; \
           --argc; \
           parsed = true; \
       }


#define OPTIONINT(O,V,MIN,MAX) \
       if ( ! parsed && j < argc && 0 == strcmp(argv[j], #O) ) \
       { \
           std::string theoption = argv[j]; \
           std::string theparm; \
           int thenum = 0; \
           bool inbounds = false; \
           for(int i=1; i<argc; ++i) \
               argv[i]  = argv[i+1]; \
           --argc; \
           if( j < argc && 0 != strncmp(argv[j], "-", (size_t) 1) ) \
           { \
              theparm = argv[j]; \
              thenum  = stoi( theparm ); \
              if( thenum >= (MIN) && thenum <= MAX ) \
              { \
                 (V) = thenum; \
           if( loglevel > 6 ) cout << "Parsed argv[" << j << "]='" << theoption << "' with value '" << theparm \
                            << "' option ( " << #O << " ). Range [" << (MIN) << "," << (MAX) << "]" \
                            << "  Setting " << #V << " to " << (V) << std::endl; \
              } \
              for(int i=1; i<argc; ++i) \
                  argv[i]  = argv[i+1]; \
              --argc; \
           } \
           if( thenum < (MIN) || thenum > (MAX) ) \
           { \
              cout << "ERROR: option " << theoption << " should be between " << (MIN) << " and " << (MAX) << std::endl; \
              return 1; \
           } \
           parsed = true; \
       }


#define OPTIONINTCSV(O,V,MIN,MAX) \
       if ( ! parsed && j < argc && 0 == strcmp(argv[j], #O) ) \
       { \
           std::string theoption = argv[j]; \
           std::string theparm; \
           std::vector<int> thenums; \
           bool inbounds = false; \
           for(int i=1; i<argc; ++i) \
               argv[i]  = argv[i+1]; \
           --argc; \
           if( j < argc && 0 != strncmp(argv[j], "-", (size_t) 1) ) \
           { \
              theparm = argv[j]; \
              if( theparm == "all" ) theparm = to_string(MIN) + "-" + to_string((MAX) - 1); \
              if( theparm.size() > 0 && theparm[theparm.size()-1] == '-' ) theparm += to_string((MAX) - 1); \
              if( theparm.size() > 0 && theparm[0] == '-' ) theparm.insert(0, to_string((MIN))); \
              thenums  = findIntList( theparm ); \
              if( thenums.size() >= (MIN) && thenums.size() <= MAX ) \
              { \
                 (V) = thenums; \
           if( loglevel > 6 ) cout << "Parsed argv[" << j << "]='" << theoption << "' with value '" << theparm \
                            << "' option ( " << #O << " ). Range [" << (MIN) << "," << (MAX) << "]" \
                            << "  Setting " << #V << " to " << vi2csv(V) << std::endl; \
              } \
              for(int i=1; i<argc; ++i) \
                  argv[i]  = argv[i+1]; \
              --argc; \
           } \
           if( thenums.size() < (MIN) || thenums.size() > (MAX) ) \
           { \
              cout << "ERROR: option " << theoption << " list size should be between " << (MIN) << " and " << (MAX) << std::endl; \
              return 1; \
           } \
           parsed = true; \
       }


#define OPTIONFLOAT(O,V,MIN,MAX) \
       if ( ! parsed && j < argc && 0 == strcmp(argv[j], #O) ) \
       { \
           std::string theoption = argv[j]; \
           std::string theparm; \
           float thenum = 0; \
           bool inbounds = false; \
           for(int i=1; i<argc; ++i) \
               argv[i]  = argv[i+1]; \
           --argc; \
           if( j < argc && 0 != strncmp(argv[j], "-", (size_t) 1) ) \
           { \
              theparm = argv[j]; \
              thenum  = stof( theparm ); \
              if( thenum >= (MIN) && thenum <= MAX ) \
              { \
                 (V) = thenum; \
           if( loglevel > 6 ) cout << "Parsed argv[" << j << "]='" << theoption << "' with value '" << theparm \
                            << "' option ( " << #O << " ). Range [" << (MIN) << "," << (MAX) << "]" \
                            << "  Setting " << #V << " to " << (V) << std::endl; \
              } \
              for(int i=1; i<argc; ++i) \
                  argv[i]  = argv[i+1]; \
              --argc; \
           } \
           if( thenum < (MIN) || thenum > (MAX) ) \
           { \
              cout << "ERROR: option " << theoption << " should be between " << (MIN) << " and " << (MAX) << std::endl; \
              return 1; \
           } \
           parsed = true; \
       }

//
// Allow output of vectors in the format {_,_,_,....,_}
//
template <typename S>
ostream& operator<<(ostream& os,
                    const vector<S>& vector)
{
    // Printing the vector as {_,_,_,...,}
    os << "{";
    for ( int i=0; i < vector.size(); i++ )
    {
       auto element = vector[i];
       os << element;
       if( i+1 != vector.size() ) os << ","; // add comma if not final element
    }
    os << "}";  
    return os;
}


struct song { int collection_offset;  // Member (int variable)
              int song_offset;        // Member (string variable)
              int data_length;
            };       // Structure variable 

const int MIN_CARD_SIZE            = 1048576;  // From actual cards seen so far
const int MAX_CARD_SIZE            = 1048576;
const int MAX_COLLECTIONS          = 100;
const int MAX_SONGS_PER_COLLECTION = 32;
const int MAX_MIDI_LENGTH          = 10000;
const int MAX_CMD_LENGTH           = 256; 
const int SONG_HEADER_LENGTH       = 2 * 16;
const int COLLECTION_HEADER_LENGTH = 7 * 16;

song songs[MAX_SONGS_PER_COLLECTION * MAX_COLLECTIONS];
int num_songs = 0;


//
// findIntList() - takes string containing a comma separated list of INTs and returns a vector of ints
// 
std::vector<int> findIntList ( std::string s )
{
   std::vector<int> vect;
   std::string recon;
   std::stringstream ss(s);  // get string stream of input string
   // reads an int from ss into i, until it cannot.    
   for (int i=0; ss >> i;)
   {
      // handle 'N-M' case like ...,4-21,... 4 is added on prior itteration so add N+1 thru M
      if( i < 0 )
      {
         if( vect.size() == 0 ) vect.push_back( 0 ); // add a leading 0 to the list if list empty
         for( int j = vect.back() + 1; j < abs(i); j++ ) vect.push_back( j ); // for N+1...M-1
         i = abs(i); // "remove" the '-' 
      }     
      vect.push_back(i);    
      if (ss.peek() == ',')
          ss.ignore();
   }
   // sort and deduplicate  
   sort( vect.begin(), vect.end() );
   vect.erase( unique( vect.begin(), vect.end() ), vect.end() );
   if( loglevel > 8 ) cout << "findIntList( '" << s << "' ) = " << vect << std::endl;   
   return vect;
}    



// Return a comma separated string of ranges, e.g. {1,3,5,7,8,9,10,...18,100,106} --> "1,3,5,7-18,100,106"
std::string vi2csv( std::vector<int> vi )
{
   // handle edge cases
   if( vi.size() == 0 ) return "";
   if( vi.size() == 1 ) return to_string( vi[0] );
   if( vi.size() == 2 ) return to_string( vi[0] ) + "," + to_string( vi[1] );
   std::string retval = "";
   int idx = 0;
   int cur = 0;
   while( idx < vi.size() )
   {
      retval += to_string(vi[idx] ); // add vi[idx] 
      cur = idx;
      // while vi[idx+1] INCREMENTS by one
      while( idx + 1 < vi.size() && vi[idx] == vi[idx+1] - 1 )
      {
         idx++;
      }
      if( idx > cur )
      {
         retval += "-" + to_string( vi[idx] );
      }
      idx++;
      if( idx < vi.size() ) retval += ",";
   }
   return retval;
}



std::string HexToBytes(const std::string &hex)
{
  // Return value is a string.  Caller will use string:data() to get the byte stream
  // If a variable ?? is encountered, substitute the current try value 
  std::string bytes;
  char byte;

  for(unsigned int i = 0; i < hex.length(); i += 2)
  {
    std::string byteString = hex.substr(i, 2);
    if( byteString == "??" )
    {
        byte = (char) currenttry;
    }
    else
    {
        byte = (char) strtol(byteString.c_str(), NULL, 16);
    }
    bytes.push_back(byte);
  }

  return bytes;
}           


//
// Play AutoBel Card track data ( and quasimodo patch data )
// returning the computed length of the track.   The getsize 
// option silently scans the data only and returns the computed
// length, performing any error checking, but without actually
// sending the notes.
//
int autobelplayer( char buffer[], bool getsize )
{
   char spinner[] = "   >   ";
   char bytes[4];
   unsigned int spinner_count = 3;
   unsigned int col_offset = 0;  // for direct stream playing
   unsigned int song_offset = 0; // for direct stream playing      
   unsigned int midi_offset = song_offset;  // Removed +32 offset as we now accept the stream
   unsigned int midi_length = 0;
   int duration = 0;  // the elapsed time ( sum of all delay operations in microseconds ) 

   if( ! getsize ) printf( "\n" );

   if( debug )
   {   
       std::cout << "Data: ";
       bool done = false;
       for (int i = 0; ! done; ++i)
       {
           for (int j = 0; ! done; ++j)
           {
                printf("%02X ", (unsigned char)buffer[(i*16) + j]);
                if(    (i*16) + j > 1 
                    && (unsigned char)buffer[(i*16) + j - 1] == 0xFF 
                    && (unsigned char)buffer[(i*16) + j] == 0x2F ) done = true;
           }
           printf("\n      ");
       }
       printf("\n");
       fflush(stdout);
   }

   while( midi_offset + midi_length < song_offset + MAX_MIDI_LENGTH )
   {
       // detect FF 2F end of MIDI data signature and terminate.
       if(    (int)(unsigned char)buffer[col_offset+midi_offset+midi_length]   == 0xFF 
           && (int)(unsigned char)buffer[col_offset+midi_offset+midi_length+1] == 0x2F )
         {
             if( getsize ) break;  // be silent if only computing the duration
             
             printf( "  End of Song %05d: ", midi_length );
             printf( "%02X %02X ", 
             (unsigned char)buffer[col_offset+midi_offset+midi_length], 
             (unsigned char)buffer[col_offset+midi_offset+midi_length+1]);
             printf( " <-- END OF SONG " ); 
             fflush(stdout);
             break;
         } // If END

       if( debug && ! getsize ) { printf("."); fflush(stdout); }

       // TODO: Merge with section for sysexec patches
       // detect MIDI note off 8n XX XX DD / note 9n XX XX DD / control Bn XX XX DD  
       if(    ((unsigned char)buffer[col_offset+midi_offset+midi_length] & 0xF0 ) == 0x80 
           || ((unsigned char)buffer[col_offset+midi_offset+midi_length] & 0xF0 ) == 0x90
           || ((unsigned char)buffer[col_offset+midi_offset+midi_length] & 0xF0 ) == 0xB0 )
         {         
         
             if( ! getsize ) printf( "      Playing %05d: ", midi_length );
             if( ! getsize ) printf( "%02X %02X %02X ", 

             (unsigned char)buffer[col_offset+midi_offset+midi_length], 
             (unsigned char)buffer[col_offset+midi_offset+midi_length+1],
             (unsigned char)buffer[col_offset+midi_offset+midi_length+2]);

             bytes[0]=(unsigned char)buffer[col_offset+midi_offset+midi_length];
             bytes[1]=(unsigned char)buffer[col_offset+midi_offset+midi_length+1];
             bytes[2]=(unsigned char)buffer[col_offset+midi_offset+midi_length+2];
             bytes[3]=(unsigned char)delimiter;
                        
             midi_length = midi_length + 3;  // Move offset to the "delay" byte

             int delay = 0;
             // if delay byte > 0x80, it is a MSB ( less 0x80 ) for a 2 byte delay value, I guess
             if( (int)(unsigned char)buffer[col_offset+midi_offset+midi_length] > 0x80 )
             {
                  delay = (128 * ((int)(unsigned char)buffer[col_offset+midi_offset+midi_length] - 0x80));
                  midi_length++;
             }
             delay = delay + (int)(unsigned char)buffer[col_offset+midi_offset+midi_length];
             midi_length++;
             if( ! getsize ) printf( "with delay %5d ", delay );
             // Calculate delay specified, then subtract propigation delay for 40 bits @ line speed
             float usdelay = ( ( delay * 1000000 ) / tempo ) - ( ( 1000000 * 4 * 10 ) / bps );
             if( ! getsize ) printf( " ( %7d us ) ", (int)usdelay );

             if( tty && ! getsize )
             {
                 int msg_len = 3;
                 if( delimited ) msg_len++;
                 write(serial, &bytes[0], msg_len);
                 printf( "--> %02X %02X %02X %02X ", 
                         (unsigned char)bytes[0], 
                         (unsigned char)bytes[1], 
                         (unsigned char)bytes[2], 
                         (unsigned char)bytes[3] );  //  <-- This 4th byte is not needed if not delimited
                 printf( "--> TTY " ); 
             }
                        
             if( ! getsize ) printf( "\r" );
             if( debug && ! getsize ) printf( "\n" );
             fflush(stdout);                      
                      
             // if( usdelay > 100 ) usleep( (int) usdelay );
             while(usdelay > 100)
             {
                 int thedelay;
                 if( usdelay > 250000 ) { thedelay = 250000; } else { thedelay = usdelay; }
                 if( ! getsize ) printf( " %.4s\r", &spinner[ spinner_count % 4 ] );
                 if( ! getsize ) fflush(stdout);
                 duration = duration + thedelay;
                 if( ! getsize ) usleep( thedelay );
                 usdelay = usdelay - thedelay;
                 spinner_count--;
             } // while Delay spinner                        
       } // If 9n XX XX DD / Bn XX XX DD / F0 XX XX DD                                                                                  
       else
       // Fn XX ... XX FF  where n=0,1,2, and 6 
       if(    ((unsigned char)buffer[col_offset+midi_offset+midi_length] & 0xFF ) == 0xF0
           || ((unsigned char)buffer[col_offset+midi_offset+midi_length] & 0xFF ) == 0xF1
           || ((unsigned char)buffer[col_offset+midi_offset+midi_length] & 0xFF ) == 0xF2
           || ((unsigned char)buffer[col_offset+midi_offset+midi_length] & 0xFF ) == 0xF6 )
         {         
         
             // All Fn XX ... XX sequences are delimited with 0xFF ( my us, in the patch file
             // no Fn XX ... XX sequences are observed in AutoBel card data, it is sent by the
             // keyboard or the console.   These sequences do not appear to be any standard
             // MIDI commands.
             
             int cmdlen = 0;
             while(    cmdlen < MAX_CMD_LENGTH
                    && midi_offset + midi_length + cmdlen < song_offset + MAX_MIDI_LENGTH
                    && ((unsigned char)buffer[col_offset+midi_offset+midi_length+cmdlen] & 0xFF ) != 0xFF
                  ) cmdlen++;

             // The above does not include the 0xFF delimiter, copy it anyway eve if not used later.
             bytes[cmdlen]=(unsigned char)buffer[col_offset+midi_offset+midi_length+cmdlen];
                 
             if( ! getsize ) printf( "     Patching %05d: ", midi_length );

             for( int i = 0; i < cmdlen; i++ )
             {
                 if( ! getsize ) printf( "%02X ", 
                         (unsigned char)buffer[col_offset+midi_offset+midi_length+i]);
                 bytes[i]=(unsigned char)buffer[col_offset+midi_offset+midi_length+i];
             }

             midi_length = midi_length + cmdlen + 1; // advance offset past this command AND the 0xFF delimiter

             // Fn XX ... XX commands do not have a delay ( yet ) 
             int delay = 0;
             /* 
             // if delay byte > 0x80, it is a MSB ( less 0x80 ) for a 2 byte delay value, I guess
             if( (int)(unsigned char)buffer[col_offset+midi_offset+midi_length] > 0x80 )
             {
                  delay = (128 * ((int)(unsigned char)buffer[col_offset+midi_offset+midi_length] - 0x80));
                  midi_length++;
             }
             delay = delay + (int)(unsigned char)buffer[col_offset+midi_offset+midi_length];
             midi_length++;
             printf( " with delay %5d ", delay );
             // Calculate delay specified, then subtract propigation delay for 40 bits @ line speed
             float usdelay = ( ( delay * 1000000 ) / tempo ) - ( ( 1000000 * 4 * 10 ) / bps );
             printf( " ( %7d us )", (int)usdelay );
             */
             float usdelay = 0; // set to zero since we are not doing a delay
             
             if( tty && ! getsize )
             {
                 // Fn XX ... XX commands are delimited with 0xFF, drop it if needed
                 if( delimited ) cmdlen++; 
                 printf( "--> " );
                 for( int i = 0; i < cmdlen; i++ )
                 {
                     write(serial, &bytes[i], 1);
                     printf( "%02X ", 
                             (unsigned char)bytes[i]);
                 }
                 printf( "--> TTY " ); 
             }
                        
             if( ! getsize ) printf( "\r" );
             if( debug ) printf( "\n" );
             fflush(stdout);                      
                      
             // if( usdelay > 100 ) usleep( (int) usdelay );
             while(usdelay > 100)
             {
                 int thedelay;
                 if( usdelay > 250000 ) { thedelay = 250000; } else { thedelay = usdelay; }
                 if( ! getsize ) printf( " %.4s\r", &spinner[ spinner_count % 4 ] );
                 if( ! getsize ) fflush(stdout);
                 usleep( thedelay );
                 usdelay = usdelay - thedelay;
                 spinner_count--;
             } // while Delay spinner                        
        } // F0 XX ... XX FF                                                                                  
        else
        {
             printf( "        Error %05d: ", midi_length );
             printf( "%02X <-- UNKNOWN COMMAND", 
             (unsigned char)buffer[col_offset+midi_offset+midi_length]);
             midi_length++; // Advance 1 byte.
             printf( "\r" );
             if( debug ) printf( "\n" );
             fflush(stdout);                              
        }


    } // while Playing               
    // Clear the Player line
    fflush(stdout);
    if( ! getsize ) cout << "\r                                                                        \r";
    cout.flush();
   return duration;
} // if play_song


bool autobelplayer( char buffer[] )
{
    int duration = autobelplayer( &buffer[0], false ); // actually play the song
    return true;
}


bool patchplayer( std::string name )
{
    bool retval = false;
    int idx = 0;
    if( debug ) cout << "patchplayer( '" << name << "' ):" << std::endl; 
    while( idx < patchname.size() )
    {
       if( patchname[idx] == name )
       {
           if( debug )
           {   
               cout << "Patch `" << patchname[idx] << "' = '" << patchdata[idx] << std::endl; 
               std::cout << "Data: ";

               for (int i = 0; (i*16) < HexToBytes( patchdata[idx].data() ).size(); ++i)
               {
                   for (int j = 0; j < 16 && (i*16)+j < HexToBytes( patchdata[idx].data() ).size(); ++j)
                       printf("%02X ", (unsigned char)HexToBytes( patchdata[idx].data() ).data()[(i*16) + j]);
                   printf("\n      ");
               }
               printf("\n");
               fflush(stdout);
           }
           
           if( debug ) cout << "autobelplayer():" << std::endl;  
           retval = autobelplayer(HexToBytes( patchdata[idx].data() ).data());
           break;
       }
       idx++;
    }
    return retval;
}


int main(int argc, char** argv) {
    std::string cmd = argv[0];

// - - - - Parse CLI Arguments - - - -

if( argc > 0 )
{
    std::string current_option;
    int j;
    j = 1;
    while( j < argc )
    {
       bool parsed = false;
       
       if ( ! parsed && j < argc ) current_option = argv[j]; 
       
       OPTIONBOOL(--debug,debug,true);
       if( debug ) loglevel = 7;

       OPTIONBOOL(--help,showhelp,true);
       OPTIONBOOL(-h,showhelp,true);
       
       OPTIONBOOL(--play,play,true);
       OPTIONBOOL(--tty,tty,true);
       OPTIONBOOL(--delimited,delimited,true);
       OPTIONBOOL(--play,play,true);
       OPTIONBOOL(--listcolls,listcolls,true);
       OPTIONBOOL(--listcollections,listcolls,true);
       OPTIONBOOL(--lc,listcolls,true);
       OPTIONBOOL(--listsongs,listsongs,true);
       OPTIONBOOL(--ls,listsongs,true);

       OPTIONINT(--collection,collection_id,1,10000);
       OPTIONINT(--song,song_id,0,31);      

       OPTIONFLOAT(--tempo,tempo,1.0f,10000.0f);
       OPTIONFLOAT(--transpose,transpose,-88.0f,88.0f);

       OPTIONINTCSV(--try,tryvalue,0,256);

       // is it --  ( i.e. stop parsing options ) 
       if ( ! parsed && j < argc && ( 0 == strcmp(argv[j], "--") || 0 == strcmp(argv[j], "--ignore_rest") ) )
       {
           if( loglevel > 6 ) cout << "Parsed " << argv[j] << " option ( -- )" << std::endl;
           for(int i=1; i<argc; ++i)
               argv[i]  = argv[i+1];
           --argc;
           parsed = true;
           j = argc + 1; // set end condition for command line parsing
       }


       // is it -o output ?
       if ( ! parsed && j < argc && 0 == strcmp(argv[j], "-o") )
       {
           std::string theoption = argv[j];
           for(int i=1; i<argc; ++i)
               argv[i]  = argv[i+1];
           --argc;
           // look for required parameter and ensure it does not begin with a "-" 
           if( j < argc && 0 != strncmp(argv[j], "-", (size_t) 1) )
           {
              outFile = argv[j];
              if( loglevel > 6 ) cout << "Parsed " << theoption << " " << outFile << std::endl;
              // remove the file name
              for(int i=1; i<argc; ++i)
                  argv[i]  = argv[i+1];
              --argc;
           }
           else
           {
              cout << "ERROR: option "<< theoption << " requires a filename." << std::endl;
              return 1;              
           }

           parsed = true;
       }

       // is it --patch
       if ( ! parsed && j < argc && 0 == strcmp(argv[j], "--patchin") )
       {
           std::string theoption = argv[j];
           for(int i=1; i<argc; ++i)
               argv[i]  = argv[i+1];
           --argc;
           // look for required parameter and ensure it does not begin with a "-" 
           if( j < argc && 0 != strncmp(argv[j], "-", (size_t) 1) )
           {
              patchin.push_back( argv[j] );
              if( loglevel > 6 ) cout << "Parsed " << theoption << " " << patchin[patchin.size() -1] << std::endl;
              // remove the patch name
              for(int i=1; i<argc; ++i)
                  argv[i]  = argv[i+1];
              --argc;
           }
           else
           {
              cout << "ERROR: option "<< theoption << " requires a patch name." << std::endl;
              return 1;              
           }

           parsed = true;
       }

       // is it --patchout
       if ( ! parsed && j < argc && 0 == strcmp(argv[j], "--patchout") )
       {
           std::string theoption = argv[j];
           for(int i=1; i<argc; ++i)
               argv[i]  = argv[i+1];
           --argc;
           // look for required parameter and ensure it does not begin with a "-" 
           if( j < argc && 0 != strncmp(argv[j], "-", (size_t) 1) )
           {
              patchout.push_back( argv[j] );
              if( loglevel > 6 ) cout << "Parsed " << theoption << " " << patchout[patchout.size() -1] << std::endl;
              // remove the patch name
              for(int i=1; i<argc; ++i)
                  argv[i]  = argv[i+1];
              --argc;
           }
           else
           {
              cout << "ERROR: option "<< theoption << " requires a patch name." << std::endl;
              return 1;              
           }

           parsed = true;
       }
       
       // is it --patch
       if ( ! parsed && j < argc && 0 == strcmp(argv[j], "--patch") )
       {
           std::string theoption = argv[j];
           for(int i=1; i<argc; ++i)
               argv[i]  = argv[i+1];
           --argc;
           // look for required parameter and ensure it does not begin with a "-" 
           if( j < argc && 0 != strncmp(argv[j], "-", (size_t) 1) )
           {
              patches.push_back( argv[j] );
              if( loglevel > 6 ) cout << "Parsed " << theoption << " " << patches[patches.size() -1] << std::endl;
              // remove the patch name
              for(int i=1; i<argc; ++i)
                  argv[i]  = argv[i+1];
              --argc;
           }
           else
           {
              cout << "ERROR: option "<< theoption << " requires a patch name." << std::endl;
              return 1;              
           }

           parsed = true;
       }

       // if an option was not parsed AND there are more items, its likely a bad option.
       if( ! parsed && argc > 2 )
       {
           cout << "ERROR: command line option '" << current_option << "' not valid." << std::endl;
           cout << "       use " << argv[0] << " -h for help." << std::endl;
       }

       if( ! parsed ) j++; // If an option was not found, advance the 
    }
}
  
  // At this point argv[0] is the original argv[0] e.g. ./cvfind
  // and argv[1] should be the last and final argument e.g. the input.pto file name

  // Check argument count
  // Don't report an error patches are being played
  if( argc < 2 && patchin.size() + patches.size() + patchout.size() == 0 )
    {
        cout << "After parsing options, no input file name specified." << std::endl << std::endl;
    }
    else
    {
       // But do ensure there is actually an argument for the filename
       if( argc > 1 ) inFile = argv[1];
    }
       
  // Check argument count
  if( argc > 2 )
    {
        cout << "After parsing options, there were extraneous arguments sprcified." << std::endl << std::endl;
        // display any effective arguments ( excluding popped ones )
        cout << "Extraneous arguments:" << std::endl;
        // Print arguments because I have not coded in 20 years 	
        int i = 2;
        while ( i < argc )
        {
           cout << "[ " << i << " ]" << argv[i] << std::endl;
           i++;
        }
        cout << std::endl;  
    }  
      
  // Check argument count if there is no file name specified, and no patches specified, report error and help
  if( argc != 2 && patchin.size() + patches.size() + patchout.size() == 0 )
    {
        if( showhelp )
        {
        cout << "Description: " << std::endl;
        cout << "   This is a swiss army knife for Schulmerich brand Gen 4 ( possibly earlier )" << std::endl;
        cout << "   Electronic Carillon systems.  It can read AutoBel Card image files, display their" << std::endl;
        cout << "   contents, and sequence song data to a serial port for playback.   The MIDI data by" << std::endl;
        cout << "   default includes 0xFF delimiters required by the Gen 4 system.  Serial output is intended" << std::endl;
        cout << "   to be sent out a USB to TTL adapter which itself is attached to an easily constructed" << std::endl;
        cout << "   fiber optic transciever that injects data into the system." << std::endl;
        cout << std::endl;
        }
        cout << "Usage: " << argv[0] << " [-d] [-h] [--option ...] [ -o output] [ input.dd ]" << std::endl;
        cout << std::endl;

        cout << "   Ex: " << argv[0] << " --ls myautobelcard.bin   Lists card contents" << std::endl;
        cout << "       " << argv[0] << " --play --ttl --song 7 --collection 1152 card.dd  Plays collection 1152, song 07" << std::endl;
        cout << "       " << argv[0] << " --play --ttl --collection 908 songs.dd  Plays all songs in collection 908" << std::endl;
        if( showhelp )
        {
        cout << "       " << argv[0] << " --lc encode_backup.dat   Lists card collections" << std::endl;
        cout << "       " << argv[0] << " --tempo 100 --play --ttl --collection 908 songs.dd  Plays them 4x faster!" << std::endl;
        cout << "       " << argv[0] << " --ls --tempo 100 songs.dd  Lists songs, showing expected run times at 4x" << std::endl;
        }
        cout << std::endl;

        cout << " Args:  --help / -h         Show help and additional info for all options" << std::endl;
        cout << "        --ls / --listsongs  Lists songs on the specified AutoBel Card image file" << std::endl;
        cout << "        --lc                Lists collections on the specified AutoBel Card image file" << std::endl;
        if( showhelp )
        {
        cout << "        --lp                List patches in the quasimodo.patches file" << std::endl;
        }
        cout << "        --song {list}       Specifies a list of song numbers, as seen in --ls" << std::endl;
        cout << "        --collection {list} Specifies a list of collection numbers, as seen in --ls" << std::endl;
        cout << "                            {list} is a comma separated list of integers.  e.g. 5 or 2,9,3" << std::endl;
        cout << "                            ranges can also be specified, e.g. 990,998-1007,1152" << std::endl;
        cout << "                            Specifying only a song or collection list acts as a filter." << std::endl;
        cout << "        --play              Plays AutoBel Card Songs and / or patches specified" << std::endl;
        cout << "                            This is essentiall a dry-run for testing playability, see --ttl" << std::endl;
        cout << "        --tty               Used with --play to actually send MIDI" << std::endl;
        if( showhelp )
        {
        cout << "        --tempo             Number of delay units per second.  25 = default" << std::endl;
        cout << "        --patchin           Plays the specified patch as a leadin" << std::endl;
        cout << "        --patches           Plays the specified patch as if it were a song" << std::endl;
        cout << "        --patchout          Plays the specified patch as a leadout" << std::endl;
        cout << "                            Patches can be played alone when no input.dd file is specified" << std::endl;
        cout << "        --try {list}        Specifies a list of integers to substitute for patch variables" << std::endl;
        cout << "                            ( \"??\" in the .patches file ) to automate trying different value" << std::endl;
        cout << "                            e.g. for patch \"90 ?? 7F 18\" with --try 64-96, the patch is played" << std::endl;
        cout << "                            for each note value from 64 thru 96.  A ocatonic series can be easily" << std::endl;
        cout << "                            sequenced by using --try 40,41,43,44,46,47,49,50 or it can be used to" << std::endl;
        cout << "                            try different parameters in control commands like \"B0 01 ?? 00\" etc." << std::endl;
        cout << "        --debug             Vomits forth additional details" << std::endl;
        cout << "        -o output           Saves output, if any to specified file" << std::endl;
        }
        cout << "        input.dd            Input file, a binary image file of the AutoBel / Encode Backup Card" << std::endl;
        return 1;
    }

// - - - - - Display "try values" list

if( tryvalue.size() != 0 ) cout << " Will Try Values: " << vi2csv( tryvalue ) << std::endl;

// - - - - - Show leadin, play, leadout patching
   if( debug )
   {
       cout << " Lead-in Patch Sequence: " << patchin << std::endl;
       cout << "    Play Patch Sequence: " << patches << std::endl;
       cout << "Lead-out Patch Sequence: " << patchout << std::endl;
   }

// - - - - - Read Patch Table Into Memory - - - - -

    //
    // Patch file format:
    //
    // # Comment
    // patch_name:patch_data
    // :more_patch_data
    // 
    //   # Super Bells Are The Best Bells
    // SUPER BELL : B0 01 03 10 B0 05 3F 10 # Setup channel one
    //              B1 01 03 10 B1 05 3F 10 # setup channel two
    //
    // All spaces are removed.  All data after a # is ignored.  A name before a ':'
    // starts a new patch entry.  Data after ':' is added to the current patch.
    //
    if( patchload )
    {
        std::string str ;
        int line = 0;
        std::fstream file;
        file.open(patchFile,std::ios::in);
         
        while(getline(file, str))
        {
            line++;
            if( debug ) cout << patchFile << " : " << line << " : '" << str << "'" << std::endl; 
            // str = std::regex_replace(std::regex_replace(str, std::regex("^ +| +$|(\\S+)"), "$1"), std::regex(" {3,}"), "  ");
            // str.erase(std::remove_if(str.begin(), str.end(), std::isspace),str.end());
            str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
            if( debug ) cout << " ( Remove Spaces ) --> '" << str << "'" << std::endl; 
            // If string is too short, or is a comment, skip it.
            if( str.size() < 4 || str[0] == '#' ) continue;

            // Remove comment and anything after
            int idx_pound = str.find("#");
            if( idx_pound != -1 ) str.erase(str.find ("#"), str[str.length() - 1]);      
            if( debug ) cout << " ( Remove Comment ) --> '" << str << "'" << std::endl; 

            // If string is too short, or is a comment, skip it.
            if( str.length() < 2 ) continue;
            
            // Does string contain a colon?
            int idx_colon = str.find(":");
            // If there is a colon, parse into patch name and patch data and start a new patch
            if (idx_colon != -1)
            {
                // TODO: Fails when data is colon prefixed, but has no name
                if (str.length() > idx_colon)
                   patchdata.push_back(str.substr(idx_colon + 1));
                str.erase(str.find (":"), str[str.length() - 1]); // Erase the colon and everything after
                if( str.length() > 0 ) patchname.push_back(str);
                   if( debug ) cout << "Patch '" << patchname[patchname.size()-1] << "' = '"
                        << patchdata[patchdata.size()-1] << "'" << std::endl;
                        
            }
            else
            {
               // When no colon ( patch name prefix is not present ) add string to the last patch 
               if( patchname.size() < 1 )
               {
                  // If there was no last patch, error out.
                  if( debug ) cout << "Error: " << patchFile << " : " << line << " : No prior patch name for '" 
                       << str << "'" << std::endl;
                  continue;     
               }
               else
               {
                   patchdata[patchdata.size()-1] = patchdata[patchdata.size()-1] + str;
                   if( debug ) cout << "Patch '" << patchname[patchname.size()-1] << "' = '"
                        << patchdata[patchdata.size()-1] << "'" << std::endl;
               }        
            }
        }

        if( debug ) cout << "Info: Loaded " << patchname.size() << " MIDI patches from " << patchFile << std::endl;
        if( debug ) cout << "Loaded Patches: " << patchname << std::endl;

    }

// - - - - - Open Input File - - - - - 

    if( inFile.size() > 0 )
    {  
       std::ifstream infile(inFile, std::ifstream::binary);
       if (! infile )
       {
           std::cout << "Error: could not open file " << inFile << std::endl;
           return -1; 
       }
       else
       {
           // We have a open file!
           
           cardsize = std::filesystem::file_size(inFile);
            
           if( debug ) std::cout << "Card Size is supposedly " << cardsize << " bytes." << std::endl;
    
           if( cardsize < MIN_CARD_SIZE )
           {
               std::cout << "Error: Card Size " << cardsize << " is less than " << MIN_CARD_SIZE << std::endl;
               return -1; 
           }

           if( cardsize > MAX_CARD_SIZE )
           {
               std::cout << "Error: Card Size " << cardsize << " greater than " << MAX_CARD_SIZE << std::endl;
               return -1; 
           }
    
           // Allocate a string, make it large enough to hold the input

           cardbuffer.resize(cardsize);
    
           // read the text into the string.  This is totally stupid as read() does not return the number of bytes read.
           // we just have to "assume" it worked.
           infile.read(&cardbuffer[0],  cardbuffer.size() );
           infile.close();   

           if( debug ) std::cout << "Read " << cardsize << " from " << inFile << std::endl;

        }
    }
    else
    {
       // return 0;  // Continue anyway
    }    
    

/*
    std::cout << "Header:" << std::endl;
    for (int i = 0; i < 16; ++i)
    {
       for (int j = 0; j < 16; ++j)
          printf("%02X ", (unsigned char)cardbuffer[(i*16) + j]);
       printf("\n");
    }
*/

// - - - - - - - - - - - - Serial Comms Setup - - - - - - - - - - - -

	// c++ Forbidden...
	// char* modem_device = "/dev/ttyS0";

	serial = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY ); 

	if (serial < 0) 
	{
	    if( tty )
	    {
	        // Emit an error only if the user has specified the --tty option
	        // TODO: only setup the serial port if the user specifies --tty
		cout << "Error: cannot open serial device" << std::endl; 
            } 
	}
        else
        {
            // TODO: This garbage was cut / pasted from tty2midi - it works, but also seems
            //       itself to have been copied as some options were odd.  But it works.
         
	    /* save current serial port settings */
	    tcgetattr(serial, &oldtio); 

	    /* clear struct for new port settings */
	    bzero(&newtio, sizeof(newtio)); 

	    /* 
	     * BAUDRATE : Set bps rate. You could also use cfsetispeed and cfsetospeed.
	     * CRTSCTS  : output hardware flow control (only used if the cable has
	     * all necessary lines. See sect. 7 of Serial-HOWTO)
	     * CS8      : 8n1 (8bit, no parity, 1 stopbit)
	     * CLOCAL   : local connection, no modem contol
	     * CREAD    : enable receiving characters
	     */
	    newtio.c_cflag = B38400 | CS8 | CLOCAL | CREAD; // CRTSCTS removed

	    /*
	     * IGNPAR  : ignore bytes with parity errors
	     * ICRNL   : map CR to NL (otherwise a CR input on the other computer
	     * will not terminate input)
	     * otherwise make device raw (no other input processing)
	     */
	    newtio.c_iflag = IGNPAR;

	    /* Raw output */
	    newtio.c_oflag = 0;

	    /*
	     * ICANON  : enable canonical input
	     * disable all echo functionality, and don't send signals to calling program
	     */
	    newtio.c_lflag = 0; // non-canonical

	    /* 
	     * set up: we'll be reading 4 bytes at a time.  ( ERRATA: This comment is wrong ) 
	     */
	    // Above comment appears to have been cut and pasted into tty2midi and was not true.
	    // The original settings were 0 & 1 respectively, causing blocking reads for 1 character. 
	    // For quasimodo: set to 0 & 0 which should provide for non-blocking read of the fd 
	    newtio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
	    newtio.c_cc[VMIN]     = 0;     /* blocking read until n character arrives */

	    /* 
	     * now clean the modem line and activate the settings for the port
	     */
	    tcflush(serial, TCIFLUSH);
	    tcsetattr(serial, TCSANOW, &newtio);
	}


// - - - - - Play Leadin Patches - - - - - - -
   if( play && patchin.size() > 0 )
   {
       cout << std::endl << "Leadin Patches: " << patchin;
       for( int idx = 0; idx < patchin.size(); idx++ )
          patchplayer( patchin[idx] );
       cout << std::endl;      
   }   


// - - - - - Play Patches ( Before AutoBel Cards ) - - - - - - -

   if( play && patches.size() > 0 )
   {
       // Are we trying to substitute values into a patch?
       if( tryvalue.size() == 0 )
       {
          // Nope, just play the patch
          cout << std::endl << "Player Patches: " << patches;
          for( int idx = 0; idx < patches.size(); idx++ )
             patchplayer( patches[idx] );
          cout << std::endl;
       }
       else
       {
           // Yup, so loop through all values to try and try them.
           for( int i=0; i < tryvalue.size(); i++ )
           {
              currenttry = tryvalue[i];
              cout << std::endl << "Player Patches:" << patches << " Trying Value " << currenttry;
              for( int idx = 0; idx < patches.size(); idx++ )
                 patchplayer( patches[idx] );
              cout << std::endl;
           }

       }
   }    
   
	// Linux-specific: enable low latency mode (FTDI "nagling off")
//	ioctl(serial, TIOCGSERIAL, &ser_info);
//	ser_info.flags |= ASYNC_LOW_LATENCY;
//	ioctl(serial, TIOCSSERIAL, &ser_info);
 
// - - - - - - Parese AutoBel Card Data Structures - - - - - - - - -

    int col_offset = 0;  // Asssume the collection starts at offset 0
    int col_index = 0;
    
    // While pointer is not ( size of cardbuffer - header size ) 
    while ( col_offset + COLLECTION_HEADER_LENGTH + SONG_HEADER_LENGTH < cardsize )
    {
        int col_type = ((int)(unsigned char)cardbuffer[col_offset+1] * (int)256 ) + (int)(unsigned char)cardbuffer[col_offset];
        int col_id   = ((int)(unsigned char)cardbuffer[col_offset+3] * (int)256 ) + (int)(unsigned char)cardbuffer[col_offset+2];
        int col_len  = ((int)(unsigned char)cardbuffer[col_offset+5] * (int)256 ) + (int)(unsigned char)cardbuffer[col_offset+4];

        if( col_type != 65535 )
        if( debug ) printf( "\nCollection %02d: Type $%02X%02X with ID %5d at offset $%06d\n",
                col_index,  
                (unsigned char)cardbuffer[col_offset+1], 
                (unsigned char)cardbuffer[col_offset],
                col_id, col_offset );

        if( col_type == 1 || col_type == 14594 )
        {  
            if( listcolls && ( collection_id == -1 || col_id == collection_id ))
               printf("Collection: \"%.32s\"\n", &cardbuffer[col_offset+8]);
        }
        else
        {
            // Is it the end of the card?
            if( col_type == 65535 )
            {
                        break; // while... more collections.
            }
            else
            {
                printf( "   Type: $%02X%02X <-- Invalid\n", 
                        (unsigned char)cardbuffer[col_offset+1], 
                        (unsigned char)cardbuffer[col_offset+1]);
                        break; // while... more collections.
            }
        } // Check collection header magick number
        
        int song_index = 0; // No songs so far.
        while( song_index < MAX_SONGS_PER_COLLECTION && col_len > 48 )
        {
            bool play_song = false;
            int song_offset =   ((int)(unsigned char)cardbuffer[col_offset+48+1+(song_index*2)] * (int)256 ) 
                              + (int)(unsigned char)cardbuffer[col_offset+48+(song_index*2)];
            if( song_offset != 0 )
            {                  
                int midi_offset = song_offset + 32;
                int midi_length = 0;
                while( midi_offset + midi_length < song_offset + MAX_MIDI_LENGTH )
                {
                    // detect FF 2F end of MIDI data signature
                    if(    (int)(unsigned char)cardbuffer[col_offset+midi_offset+midi_length]   == 0xFF 
                        && (int)(unsigned char)cardbuffer[col_offset+midi_offset+midi_length+1] == 0x2F )
                    {
                        break;
                    }
                    midi_length++;
                }
 
 
                if( ( col_id == collection_id || collection_id == -1 )
                     &&  ( song_index == song_id || song_id == -1 ) )
                {
                    play_song = true;
                }
                else
                {
                    // printf( "    " );
                }        
 
                if( ( listsongs || play ) && play_song )
                {
                   // Get the expected duration in microseconds, also scans / reports for errors.
                   int duration = autobelplayer( &cardbuffer[ col_offset + song_offset + 32 ], true );

                   if( song_index == 0 || play_song )
                   {
                       printf( "Collection % 4u: ", col_id );
                   }
                   else
                   {
                       printf( "                 " );
                   }
                   
                   printf( "Song %02d: \"%.32s\" at offset $%06d, %5d bytes MIDI data, duration %6.2f sec\n",
                           song_index,
                           &cardbuffer[col_offset+song_offset],
                           song_offset, midi_length, (float)((float)duration / (float)1000000) );
                   fflush(stdout);
                }        

                if( play_song && play )
                {
                    if( debug ) cout << "autobelplayer():" << std::endl;
                    bool retval = autobelplayer( &cardbuffer[ col_offset + song_offset + 32 ] );
                }

            }
            else
            {
                break; // while... more songs in this collection
            }
            song_index++;
        }      
        
        col_offset = col_offset + col_len; // Skip to next collection
        col_index++;           
    } 

// - - - - - Play Leadout Patches - - - - - - -

    if( play && patchout.size() > 0  )
    {
       cout << "Leadout Patches: " << patchout;
       for( int idx = 0; idx < patchout.size(); idx++ )
          patchplayer( patchout[idx] );
       cout << std::endl;   
    }


// - - - - - - - -  

    return 0;
}
