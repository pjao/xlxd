//
//  cpacketstream.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 06/11/2015.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of xlxd.
//
//    xlxd is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    xlxd is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Foobar.  If not, see <http://www.gnu.org/licenses/>. 
// ----------------------------------------------------------------------------

#include "main.h"
#include "cpacketstream.h"
#include <cstring>

////////////////////////////////////////////////////////////////////////////////////////
const char*         m_TranscoderModuleOn;
// constructor

CPacketStream::CPacketStream()
{
    m_bOpen = false;
    m_uiStreamId = 0;
    m_uiPacketCntr = 0;
    m_OwnerClient = NULL;
    m_CodecStream = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////
// open / close

bool CPacketStream::Open(const CDvHeaderPacket &DvHeader, CClient *client)
{
    bool m_findmodule = false;
    bool ok = false;
    
    // not already open?
    if ( !m_bOpen )
    {
        // update status
        m_bOpen = true;
        m_uiStreamId = DvHeader.GetStreamId();
        m_uiPacketCntr = 0;
        m_DvHeader = DvHeader;
        m_OwnerClient = client;
        m_LastPacketTime.Now();
	    
        //I check if it is "NONE"
        if ( m_TranscoderModuleOn[0] == 'N' && m_TranscoderModuleOn[1] == 'O' && m_TranscoderModuleOn[2] == 'N' && m_TranscoderModuleOn[3] == 'E' )
        {
            m_CodecStream = g_Transcoder.GetStream(this, CODEC_NONE);
        }
        else
        {  //I check if it is "ALL"
            if ( m_TranscoderModuleOn[0] == 'A' && m_TranscoderModuleOn[1] == 'L' && m_TranscoderModuleOn[2] == 'L' )
               m_CodecStream = g_Transcoder.GetStream(this, client->GetCodec());
		   
            else
            { //Else find the form
                for ( unsigned int i = 0; i < strlen(m_TranscoderModuleOn); i++ )
                { 
                    if( DvHeader.GetRpt2Module() == m_TranscoderModuleOn[i] )
                    {
                        m_findmodule = true;
                    }
                } //If you find it, active transcoding
                if ( m_findmodule )
                {
                    m_CodecStream = g_Transcoder.GetStream(this, client->GetCodec());
                    m_findmodule = false;
                }
                else
                    m_CodecStream = g_Transcoder.GetStream(this, CODEC_NONE);
            }
        }
        ok = true;
    }
    return ok;
}

void CPacketStream::Close(void)
{
    // update status
    m_bOpen = false;
    m_uiStreamId = 0;
    m_OwnerClient = NULL;
    g_Transcoder.ReleaseStream(m_CodecStream);
    m_CodecStream = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////
// push & pop
void CPacketStream::Push(CPacket *Packet)
{
    // update stream dependent packet data
    m_LastPacketTime.Now();
    Packet->UpdatePids(m_uiPacketCntr++);
    // transcoder avaliable ?
    if ( m_CodecStream != NULL )
    {
        // todo: verify no possibilty of double lock here
        m_CodecStream->Lock();
        {
            // transcoder ready & frame need transcoding ?
            if ( m_CodecStream->IsConnected() && Packet->HaveTranscodableAmbe() )
            {
                // yes, push packet to trancoder queue
                // trancoder will push it after transcoding
                // is completed
                m_CodecStream->push(Packet);
            }
            else
            {
                // no, just bypass tarnscoder
                push(Packet);
            }
        }
        m_CodecStream->Unlock();
    }
    else
    {
        // otherwise, push direct push
        push(Packet);
    }
}

bool CPacketStream::IsEmpty(void) const
{
    bool bEmpty = empty();
    
    // also check no packets still in Codec stream's queue
    if ( bEmpty && (m_CodecStream != NULL) )
    {
        bEmpty &= m_CodecStream->IsEmpty();
    }

    // done
    return bEmpty;
}

////////////////////////////////////////////////////////////////////////////////////////
// get

const CIp *CPacketStream::GetOwnerIp(void)
{
    if ( m_OwnerClient != NULL )
    {
        return &(m_OwnerClient->GetIp());
    }
    return NULL;
}

void CPacketStream::TranscoderModuleOn( const std::string TranscoderModuleOn )
{
    m_TranscoderModuleOn = TranscoderModuleOn.c_str();
}
