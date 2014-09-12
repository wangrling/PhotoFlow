/* 
   
 */

/*

    Copyright (C) 2014 Ferrero Andrea

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.


 */

/*

    These files are distributed with PhotoFlow - http://aferrero2707.github.io/PhotoFlow/

 */

#include <string.h>

#include "imageprocessor.hh"


static gpointer run_image_processor( gpointer data )
{
  PF::ImageProcessor::Instance().run();
}


PF::ImageProcessor::ImageProcessor()
{
  processing_mutex = vips_g_mutex_new();
  //requests_mutex = vips_g_mutex_new();
  //requests_pending = vips_g_cond_new();
  requests = g_async_queue_new();
  thread = vips_g_thread_new( "image_processor", run_image_processor, NULL );
}


void PF::ImageProcessor::optimize_requests()
{
  // Wait for new request
  //std::cout<<"ImageProcessor::optimize_requests(): popping queue..."<<std::endl;
  PF::ProcessRequestInfo* req = (PF::ProcessRequestInfo*)g_async_queue_pop( requests );
  //std::cout<<"ImageProcessor::optimize_requests(): ... done."<<std::endl;
  optimized_requests.push_back( *req );
  free( req );

  // Add any further request in the queue
  int length = g_async_queue_length( requests );
  //std::cout<<"ImageProcessor::optimize_requests(): queue length="<<length<<std::endl;
  for( int i = 0; i < length; i++ ) {
    //std::cout<<"ImageProcessor::optimize_requests(): popping queue again..."<<std::endl;
    req = (PF::ProcessRequestInfo*)g_async_queue_pop( requests );
    //std::cout<<"ImageProcessor::optimize_requests(): ... done."<<std::endl;
    //std::cout<<"ImageProcessor::optimize_requests(): residual queue length="
    //         <<g_async_queue_length( requests )<<std::endl;
    optimized_requests.push_back( *req );
    free( req );
  }

  // Optimize the requests by removing all requests preceding a "rebuild" one.
  //std::cout<<"ImageProcessor::optimize_requests(): optimizing queue"<<std::endl;
  for( std::deque<ProcessRequestInfo>::iterator i = optimized_requests.begin();
       i != optimized_requests.end(); i++ ) {
    if( i->request != IMAGE_REBUILD ) continue;
    i++;
    optimized_requests.erase( i, optimized_requests.end() );
    break;
  }
}


void PF::ImageProcessor::run()
{
	bool running = true;
  while( running ) {
    /*
    g_mutex_lock( requests_mutex );
    std::cout<<"ImageProcessor::run(): requests_mutex locked"<<std::endl;
    if( requests.empty() ) {      
      g_mutex_unlock( requests_mutex );
      std::cout<<"ImageProcessor::run(): requests_mutex unlocked"<<std::endl;
    */
    if( g_async_queue_length( requests ) == 0 ) {
      PF::Image* image = PF::PhotoFlow::Instance().get_active_image();
      if( image ) {
        PF::CacheBuffer* buf = image->get_layer_manager().get_cache_buffer();
        //std::cout<<"ImageProcessor::run(): buf="<<buf<<std::endl;
        if( buf ) {
          buf->step();
          if( buf->is_completed() ) {
            image->lock();
            image->do_update();
            image->unlock();
          }
          continue;
        }
      }
      /*
      std::cout<<"ImageProcessor::run(): waiting for new requests..."<<std::endl;
      g_cond_wait( requests_pending, requests_mutex );
      //std::cout<<"PF::ImageProcessor::run(): resuming."<<std::endl;
      if( requests.empty() ) {
				std::cout<<"PF::ImageProcessor::run(): WARNING: empty requests queue after resuming!!!"<<std::endl;
				g_mutex_unlock( requests_mutex );
        std::cout<<"ImageProcessor::run(): requests_mutex unlocked"<<std::endl;
				continue;
      }
      */
    }
    optimize_requests();
    while( !optimized_requests.empty() ) {
      PF::ProcessRequestInfo request = optimized_requests.front();
      optimized_requests.pop_front();
      /*
        std::cout<<"PF::ImageProcessor::run(): processing new request: ";
        switch( request.request ) {
        case IMAGE_REBUILD: std::cout<<"IMAGE_REBUILD"; break;
        case IMAGE_REDRAW_START: std::cout<<"IMAGE_REDRAW_START"; break;
        case IMAGE_REDRAW_END: std::cout<<"IMAGE_REDRAW_END"; break;
        case IMAGE_REDRAW: std::cout<<"IMAGE_REDRAW"; break;
        default: break;
        }
        std::cout<<std::endl;
      */
      //g_mutex_unlock( requests_mutex );


      // Process the request
      switch( request.request ) {
      case IMAGE_REBUILD:
        if( !request.image ) continue;
        //std::cout<<"PF::ImageProcessor::run(): locking image..."<<std::endl;
        request.image->lock();
        //std::cout<<"PF::ImageProcessor::run(): image locked."<<std::endl;
        /*
          if( (request.area.width!=0) && (request.area.height!=0) )
          request.image->do_update( &(request.area) );
          else
          request.image->do_update( NULL );
        */
        request.image->do_update( request.pipeline );
        request.image->unlock();
        request.image->rebuild_done_signal();
        //std::cout<<"PF::ImageProcessor::run(): updating image done."<<std::endl;
        break;
      case IMAGE_SAMPLE:
        if( !request.image ) continue;
        //std::cout<<"PF::ImageProcessor::run(): locking image..."<<std::endl;
        request.image->sample_lock();
        //std::cout<<"PF::ImageProcessor::run(IMAGE_SAMPLE): image locked."<<std::endl;
        if( (request.area.width!=0) && (request.area.height!=0) )
          request.image->do_sample( request.layer_id, request.area );
        request.image->sample_unlock();
        request.image->sample_done_signal();
        //std::cout<<"PF::ImageProcessor::run(IMAGE_SAMPLE): sampling done."<<std::endl;
        break;
      case IMAGE_UPDATE:
        if( !request.pipeline ) continue;
        //std::cout<<"PF::ImageProcessor::run(): updating area."<<std::endl;
        request.pipeline->sink( request.area );
        //std::cout<<"PF::ImageProcessor::run(): updating area done."<<std::endl;
        break;
      case IMAGE_REDRAW_START:
        request.sink->process_start( request.area );
        break;
      case IMAGE_REDRAW_END:
        request.sink->process_end( request.area );
        break;
      case IMAGE_REDRAW:
        // Get exclusive access to the request data structure
        //g_mutex_lock( request.mutex );
        if( !request.sink ) continue;

        //std::cout<<"PF::ImageProcessor::run(): processing area "
        //	       <<request.area.width<<","<<request.area.height
        //       <<"+"<<request.area.left<<"+"<<request.area.top
        //       <<std::endl;
        // Process the requested image portion
        request.sink->process_area( request.area );
        //std::cout<<"PF::ImageProcessor::run(): processing area done."<<std::endl;

        // Notify that the processing is finished
        //g_cond_signal( request.done );
        //g_mutex_unlock( request.mutex );
        break;
      case IMAGE_REMOVE_LAYER:
        if( !request.image ) break;
        if( !request.layer ) break;
        request.image->remove_layer_lock();
        request.image->do_remove_layer( request.layer );
        request.image->remove_layer_unlock();
        request.image->remove_layer_done_signal();
        break;
      case IMAGE_DESTROY:
        if( !request.image ) continue;
        delete request.image;
        std::cout<<"PF::ImageProcessor::run(): image destroyed."<<std::endl;
        break;
      case PROCESSOR_END:
        running = false;
        std::cout<<"PF::ImageProcessor::run(): processing ended."<<std::endl;
        break;
      default:
        break;
      }
    }
  }
}


void  PF::ImageProcessor::submit_request( PF::ProcessRequestInfo request )
{
  gpointer data = malloc( sizeof(PF::ProcessRequestInfo) );
  memcpy( data, &request, sizeof(PF::ProcessRequestInfo) );
  g_async_queue_push( requests, data );

  /*
  std::cout<<"PF::ImageProcessor::submit_request(): locking mutex."<<std::endl;
  g_mutex_lock( requests_mutex );
  std::cout<<"PF::ImageProcessor::submit_request(): pushing request."<<std::endl;
  requests.push_back( request );
  std::cout<<"PF::ImageProcessor::submit_request(): unlocking mutex."<<std::endl;
  g_mutex_unlock( requests_mutex );
  std::cout<<"PF::ImageProcessor::submit_request(): signaling condition."<<std::endl;
  g_cond_signal( requests_pending );
  */
}


PF::ImageProcessor* PF::ImageProcessor::instance = NULL;

PF::ImageProcessor& PF::ImageProcessor::Instance() { 
  if(!PF::ImageProcessor::instance) 
    PF::ImageProcessor::instance = new PF::ImageProcessor();
  return( *instance );
};

