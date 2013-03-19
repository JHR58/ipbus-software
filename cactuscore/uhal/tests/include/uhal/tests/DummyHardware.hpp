/*
---------------------------------------------------------------------------

    This file is part of uHAL.

    uHAL is a hardware access library and programming framework
    originally developed for upgrades of the Level-1 trigger of the CMS
    experiment at CERN.

    uHAL is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    uHAL is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with uHAL.  If not, see <http://www.gnu.org/licenses/>.


      Andrew Rose, Imperial College, London
      email: awr01 <AT> imperial.ac.uk

      Marc Magrans de Abril, CERN
      email: marc.magrans.de.abril <AT> cern.ch

---------------------------------------------------------------------------
*/

/**
	@file
	@author Andrew W. Rose
	@date 2013
*/

#ifndef DummyHardware_hpp
#define DummyHardware_hpp

#include "uhal/IPbusInspector.hpp"
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>

#include <map>
#include <vector>
#include <deque>


// Using the uhal namespace
namespace uhal
{
  static const uint32_t ADDRESSMASK = 0x000FFFFF;
  static const uint32_t REPLY_HISTORY_DEPTH = 5;
  static const uint32_t BUFFER_SIZE = 500;



  template< uint8_t IPbus_major , uint8_t IPbus_minor >
  class DummyHardware : public HostToTargetInspector< IPbus_major , IPbus_minor >
  {
      typedef HostToTargetInspector< IPbus_major , IPbus_minor > base_type;

    public:
      DummyHardware ( const uint32_t& aReplyDelay ) : HostToTargetInspector< IPbus_major , IPbus_minor >() ,
        mMemory ( ADDRESSMASK+1 , 0x00000000 ),
        mReplyDelay ( aReplyDelay ),
        mReceive ( BUFFER_SIZE , 0x00000000 ),
        mReply ( BUFFER_SIZE , 0x00000000 ),
        mReplyHistory( REPLY_HISTORY_DEPTH , std::make_pair( 0 , mReply ) ),
        mLastPacketHeader(0x200000f0),
        mTrafficHistory ( 16, 0x00 ),
        mReceivedControlPacketHeaderHistory ( 4 , 0x00000000 ),
        mSentControlPacketHeaderHistory ( 4 , 0x00000000 )
      {
      }

      virtual ~DummyHardware()
      {
      }

      virtual void run() = 0;

      void AnalyzeReceivedAndCreateReply ( const uint32_t& aByteCount )
      {
#ifdef BIG_ENDIAN_HACK

        if ( IPbus_major == 2 )
        {
          if ( LoggingIncludes ( Debug() ) )
          {
            log ( Notice() , "Big-Endian Hack included" );
          }

          for ( std::vector<uint32_t>::iterator lIt ( mReceive.begin() ) ; lIt != mReceive.end() ; ++lIt )
          {
            *lIt = ntohl ( *lIt );
          }
        }

#endif
        std::vector<uint32_t>::const_iterator lBegin, lEnd;

        //
        //-----------------------------------------------------------------------------------------------------------------------------------------------------------------
        if ( LoggingIncludes ( Debug() ) )
        {
          log ( Debug() , "\n=============================================== RECEIVED ===============================================" );
          HostToTargetInspector< IPbus_major , IPbus_minor > lHostToTargetDebugger;
          lBegin =  mReceive.begin();
          lEnd = mReceive.begin() + ( aByteCount>>2 );
          lHostToTargetDebugger.analyze ( lBegin , lEnd );
        }

        //-----------------------------------------------------------------------------------------------------------------------------------------------------------------
        //
        mReply.clear();
        lBegin =  mReceive.begin();
        lEnd = mReceive.begin() + ( aByteCount>>2 );
        base_type::analyze ( lBegin , lEnd );

        if( ( base_type::mPacketType == 0 ) && ( mReply.size() != 0 ) )
        {
          mReplyHistory.push_back( std::make_pair( base_type::mPacketCounter , mReply ) );
          mReplyHistory.pop_front();
        }

        //
        //-----------------------------------------------------------------------------------------------------------------------------------------------------------------
        if ( LoggingIncludes ( Debug() ) )
        {
          log ( Debug() , "\n=============================================== SENDING ===============================================" );
          TargetToHostInspector< IPbus_major , IPbus_minor > lTargetToHostDebugger;
          lBegin =  mReply.begin();
          lEnd = mReply.end();
          lTargetToHostDebugger.analyze ( lBegin , lEnd );
        }

        //-----------------------------------------------------------------------------------------------------------------------------------------------------------------
        //
#ifdef BIG_ENDIAN_HACK

        if ( IPbus_major == 2 )
        {
          if ( LoggingIncludes ( Debug() ) )
          {
            log ( Notice() , "Big-Endian Hack included" );
          }

          for ( std::vector<uint32_t>::iterator lIt ( mReply.begin() ) ; lIt != mReply.end() ; ++lIt )
          {
            *lIt = htonl ( *lIt );
          }
        }

#endif

        if ( mReplyDelay )
        {
          log ( Info() , "Sleeping for " , Integer ( mReplyDelay ) , "s" );
          sleep ( mReplyDelay );
          mReplyDelay = 0;
          log ( Info() , "Now replying " );
        }

      }

    private:
      void bot()
      {
        mReceivedControlPacketHeaderHistory.push_back ( base_type::mPacketHeader );
        mReceivedControlPacketHeaderHistory.pop_front();
        uint32_t lExpected ( IPbus< IPbus_major , IPbus_minor >::ExpectedHeader ( base_type::mType , 0 , base_type::mTransactionId ) );
        mReply.push_back ( lExpected );
        mSentControlPacketHeaderHistory.push_back ( lExpected );
        mSentControlPacketHeaderHistory.pop_front();
      }

      void ni_read ( const uint32_t& aAddress )
      {
        mReceivedControlPacketHeaderHistory.push_back ( base_type::mPacketHeader );
        mReceivedControlPacketHeaderHistory.pop_front();
        uint32_t lAddress ( aAddress );
        uint32_t lExpected ( IPbus< IPbus_major , IPbus_minor >::ExpectedHeader ( base_type::mType , base_type::mWordCounter , base_type::mTransactionId ) );
        mReply.push_back ( lExpected );
        mSentControlPacketHeaderHistory.push_back ( lExpected );
        mSentControlPacketHeaderHistory.pop_front();

        for ( ; base_type::mWordCounter!=0 ; --base_type::mWordCounter )
        {
          mReply.push_back ( mMemory.at ( lAddress & ADDRESSMASK ) );
        }
      }

      void read ( const uint32_t& aAddress )
      {
        mReceivedControlPacketHeaderHistory.push_back ( base_type::mPacketHeader );
        mReceivedControlPacketHeaderHistory.pop_front();
        uint32_t lAddress ( aAddress );
        uint32_t lExpected ( IPbus< IPbus_major , IPbus_minor >::ExpectedHeader ( base_type::mType , base_type::mWordCounter , base_type::mTransactionId ) );
        mReply.push_back ( lExpected );
        mSentControlPacketHeaderHistory.push_back ( lExpected );
        mSentControlPacketHeaderHistory.pop_front();

        for ( ; base_type::mWordCounter!=0 ; --base_type::mWordCounter )
        {
          mReply.push_back ( mMemory.at ( lAddress++ & ADDRESSMASK ) );
        }
      }

      void ni_write ( const uint32_t& aAddress , std::vector<uint32_t>::const_iterator& aIt , const std::vector<uint32_t>::const_iterator& aEnd )
      {
        mReceivedControlPacketHeaderHistory.push_back ( base_type::mPacketHeader );
        mReceivedControlPacketHeaderHistory.pop_front();
        uint32_t lAddress ( aAddress );

        while ( aIt != aEnd )
        {
          mMemory.at ( lAddress & ADDRESSMASK ) = *aIt++;
        }

        uint32_t lExpected;

        if ( IPbus_major == 1 )
        {
          lExpected = IPbus< IPbus_major , IPbus_minor >::ExpectedHeader ( base_type::mType , 0 , base_type::mTransactionId );
        }
        else
        {
          lExpected = IPbus< IPbus_major , IPbus_minor >::ExpectedHeader ( base_type::mType , base_type::mWordCounter , base_type::mTransactionId );
        }

        mReply.push_back ( lExpected );
        mSentControlPacketHeaderHistory.push_back ( lExpected );
        mSentControlPacketHeaderHistory.pop_front();
      }

      void write ( const uint32_t& aAddress , std::vector<uint32_t>::const_iterator& aIt , const std::vector<uint32_t>::const_iterator& aEnd )
      {
        mReceivedControlPacketHeaderHistory.push_back ( base_type::mPacketHeader );
        mReceivedControlPacketHeaderHistory.pop_front();
        uint32_t lAddress ( aAddress );

        while ( aIt != aEnd )
        {
          mMemory.at ( lAddress++ & ADDRESSMASK ) = *aIt++;
        }

        uint32_t lExpected;

        if ( IPbus_major == 1 )
        {
          lExpected = IPbus< IPbus_major , IPbus_minor >::ExpectedHeader ( base_type::mType , 0 , base_type::mTransactionId );
        }
        else
        {
          lExpected = IPbus< IPbus_major , IPbus_minor >::ExpectedHeader ( base_type::mType , base_type::mWordCounter , base_type::mTransactionId );
        }

        mReply.push_back ( lExpected );
        mSentControlPacketHeaderHistory.push_back ( lExpected );
        mSentControlPacketHeaderHistory.pop_front();
      }

      void rmw_sum ( const uint32_t& aAddress , const uint32_t& aAddend )
      {
        mReceivedControlPacketHeaderHistory.push_back ( base_type::mPacketHeader );
        mReceivedControlPacketHeaderHistory.pop_front();
        uint32_t lAddress ( aAddress );
        uint32_t lExpected ( IPbus< IPbus_major , IPbus_minor >::ExpectedHeader ( base_type::mType , 1 , base_type::mTransactionId ) );
        mReply.push_back ( lExpected );
        mSentControlPacketHeaderHistory.push_back ( lExpected );
        mSentControlPacketHeaderHistory.pop_front();

        if ( IPbus_major == 1 )
        {
          //IPbus 1.x returns modified value
          mMemory.at ( lAddress & ADDRESSMASK ) += aAddend;
          mReply.push_back ( mMemory.at ( lAddress & ADDRESSMASK ) );
        }
        else
        {
          //IPbus 2.x returns pre-modified value
          mReply.push_back ( mMemory.at ( lAddress & ADDRESSMASK ) );
          mMemory.at ( lAddress & ADDRESSMASK ) += aAddend;
        }
      }

      void rmw_bits ( const uint32_t& aAddress , const uint32_t& aAndTerm , const uint32_t& aOrTerm )
      {
        mReceivedControlPacketHeaderHistory.push_back ( base_type::mPacketHeader );
        mReceivedControlPacketHeaderHistory.pop_front();
        uint32_t lAddress ( aAddress );
        uint32_t lExpected ( IPbus< IPbus_major , IPbus_minor >::ExpectedHeader ( base_type::mType , 1 , base_type::mTransactionId ) );
        mReply.push_back ( lExpected );
        mSentControlPacketHeaderHistory.push_back ( lExpected );
        mSentControlPacketHeaderHistory.pop_front();

        if ( IPbus_major == 1 )
        {
          //IPbus 1.x returns modified value
          mMemory.at ( lAddress & ADDRESSMASK ) &= aAndTerm;
          mMemory.at ( lAddress & ADDRESSMASK ) |= aOrTerm;
          mReply.push_back ( mMemory.at ( lAddress & ADDRESSMASK ) );
        }
        else
        {
          //IPbus 2.x returns pre-modified value
          mReply.push_back ( mMemory.at ( lAddress & ADDRESSMASK ) );
          mMemory.at ( lAddress & ADDRESSMASK ) &= aAndTerm;
          mMemory.at ( lAddress & ADDRESSMASK ) |= aOrTerm;
        }
      }

      void unknown_type()
      {
        log ( Error() , Integer ( base_type::mHeader, IntFmt<hex,fixed>() ) , " is an unknown IPbus transaction header. Returning error code." );
        mReceivedControlPacketHeaderHistory.push_back ( base_type::mPacketHeader );
        mReceivedControlPacketHeaderHistory.pop_front();
        uint32_t lExpected ( IPbus< IPbus_major , IPbus_minor >::ExpectedHeader ( base_type::mType , 0 , base_type::mTransactionId , 1 ) );
        mReply.push_back ( lExpected );
        mSentControlPacketHeaderHistory.push_back ( lExpected );
        mSentControlPacketHeaderHistory.pop_front();
      }

      bool control_packet_header ()
      {
        if ( LoggingIncludes ( Debug() ) )
        {
          base_type::control_packet_header();
        }

        if( base_type::mPacketCounter != 0 )
        {
          uint16_t lTemp ( ( ( mLastPacketHeader>>8 ) &0x0000FFFF ) + 1 );
          if (lTemp == 0 )
          {
            lTemp = 1;
          }
  
          if( base_type::mPacketCounter != lTemp )
          {
            mTrafficHistory.push_back ( 5 );
            mTrafficHistory.pop_front();
            return false;
          }
    
          mLastPacketHeader = base_type::mPacketHeader;
        }

        mReply.push_back ( base_type::mPacketHeader );
        mTrafficHistory.push_back ( 2 );
        mTrafficHistory.pop_front();
        return true;
        
      }


      void status_packet_header ( )
      {
        if ( LoggingIncludes ( Debug() ) )
        {
          base_type::status_packet_header();
        }

        mReply.push_back ( base_type::mPacketHeader );
        mReply.push_back ( BUFFER_SIZE * sizeof ( uint32_t ) );
        mReply.push_back ( REPLY_HISTORY_DEPTH );

        uint16_t lTemp ( ( ( mLastPacketHeader>>8 ) &0x0000FFFF ) + 1 );
        if (lTemp == 0 )
        {
          lTemp = 1;
        }
        mReply.push_back ( ( mLastPacketHeader & 0xFF0000FF ) | ( ( lTemp <<8 ) & 0x00FFFF00 ) );

        std::deque< uint8_t >::const_iterator lIt ( mTrafficHistory.begin() );

        for ( uint32_t i = 0; i != 4 ; ++i )
        {
          uint32_t lTemp ( 0x00000000 );

          for ( uint32_t j = 0; j != 4 ; ++j )
          {
            lTemp <<= 8;
            lTemp |= ( uint32_t ) ( *lIt );
            lIt++;
          }

          mReply.push_back ( lTemp );;
        }

        for ( std::deque< uint32_t >::const_iterator i = mReceivedControlPacketHeaderHistory.begin(); i != mReceivedControlPacketHeaderHistory.end() ; ++i )
        {
          mReply.push_back ( *i );
        }

        for ( std::deque< uint32_t >::const_iterator i = mSentControlPacketHeaderHistory.begin(); i != mSentControlPacketHeaderHistory.end() ; ++i )
        {
          mReply.push_back ( *i );
        }

        mTrafficHistory.push_back ( 3 );
        mTrafficHistory.pop_front();
      }


      void resend_packet_header ()
      {
        if ( LoggingIncludes ( Debug() ) )
        {
          base_type::resend_packet_header();
        }

        std::deque< std::pair< uint32_t , std::vector< uint32_t > > >::reverse_iterator lIt = mReplyHistory.rbegin();

        for( ; lIt!=mReplyHistory.rend() ; ++lIt )
        {
          if( lIt->first == base_type::mPacketCounter )
          {
            mReply = lIt->second;
            break;
          }
        }

        mTrafficHistory.push_back ( 4 );
        mTrafficHistory.pop_front();
      }

      void unknown_packet_header()
      {
        mTrafficHistory.push_back ( 5 );
        mTrafficHistory.pop_front();
      }



    private:
      std::vector< uint32_t > mMemory;
      uint32_t mReplyDelay;

    protected:
      std::vector< uint32_t > mReceive;
      std::vector< uint32_t > mReply;

      //IPbus 2.0 and above only
      std::deque< std::pair< uint32_t , std::vector< uint32_t > > > mReplyHistory;
      uint32_t mLastPacketHeader;
      std::deque< uint8_t > mTrafficHistory;

      std::deque< uint32_t > mReceivedControlPacketHeaderHistory;
      std::deque< uint32_t > mSentControlPacketHeaderHistory;


  };







  struct CommandLineOptions
  {
    uint32_t delay;
    uint16_t port;
    uint32_t version;
  };

  CommandLineOptions ParseCommandLineOptions ( int argc,char* argv[] )
  {
    // Declare the supported options.
    boost::program_options::options_description desc ( "Allowed options" );
    desc.add_options()
    ( "help,h", "Produce this help message" )
    ( "delay,d", boost::program_options::value<uint32_t>()->default_value ( 0 ) , "Reply delay for first packet (in seconds) - optional" )
    ( "port,p", boost::program_options::value<uint16_t>() , "Port number to listen on - required" )
    ( "version,v", boost::program_options::value<uint32_t>() , "IPbus Major version (1 or 2) - required" )
    ( "verbose,V", "Produce verbose output" )
    ;
    boost::program_options::variables_map vm;

    try
    {
      boost::program_options::store ( boost::program_options::parse_command_line ( argc, argv, desc ), vm );
      boost::program_options::notify ( vm );

      if ( vm.count ( "help" ) )
      {
        std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl;
        std::cout << desc << std::endl;
        exit ( 0 );
      }

      CommandLineOptions lResult;
      lResult.delay = vm["delay"].as<uint32_t>();
      lResult.port = vm["port"].as<uint16_t>();
      lResult.version = vm["version"].as<uint32_t>();

      if ( vm.count ( "verbose" ) )
      {
        setLogLevelTo ( Debug() );
      }
      else
      {
        setLogLevelTo ( Notice() );
      }

      return lResult;
    }
    catch ( std::exception& e )
    {
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl;
      std::cout << desc << std::endl;
      exit ( 1 );
    }
  }


}

#endif


