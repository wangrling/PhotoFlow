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

#ifndef VIPS_RAW_PREPROCESSOR_H
#define VIPS_RAW_PREPROCESSOR_H

#include <string>

#include "../base/processor.hh"
#include "../base/rawmatrix.hh"

#include "raw_image.hh"


//#define RT_EMU 1

namespace PF 
{

  enum wb_mode_t {
    WB_CAMERA,
    WB_SPOT,
    WB_COLOR_SPOT
  }; 

  class RawPreprocessorPar: public OpParBase
  {
    dcraw_data_t* image_data;

    PropertyBase wb_mode;

    Property<float> wb_red;
    Property<float> wb_green;
    Property<float> wb_blue;

    Property<float> wb_target_L;
    Property<float> wb_target_a;
    Property<float> wb_target_b;

    Property<float> exposure;

  public:
    RawPreprocessorPar();

    /* Set processing hints:
       1. the intensity parameter makes no sense for an image, 
			 creation of an intensity map is not allowed
       2. the operation can work without an input image;
			 the blending will be set in this case to "passthrough" and the image
			 data will be simply linked to the output
		*/
    bool has_intensity() { return false; }
    bool has_opacity() { return false; }
    bool needs_input() { return true; }

    dcraw_data_t* get_image_data() {return image_data; }

    wb_mode_t get_wb_mode() { return (wb_mode_t)(wb_mode.get_enum_value().first); }
    float get_wb_red() { return wb_red.get(); }
    float get_wb_green() { return wb_green.get(); }
    float get_wb_blue() { return wb_blue.get(); }

    float get_exposure() { return exposure.get(); }

    VipsImage* build(std::vector<VipsImage*>& in, int first, 
										 VipsImage* imap, VipsImage* omap, unsigned int& level);
  };

  

  template < OP_TEMPLATE_DEF > 
  class RawPreprocessor
  {
  public: 
    void render_camwb(VipsRegion** ireg, int n, int in_first,
											VipsRegion* imap, VipsRegion* omap, 
											VipsRegion* oreg, RawPreprocessorPar* par)
    {
      dcraw_data_t* image_data = par->get_image_data();
      float exposure = par->get_exposure();
      Rect *r = &oreg->valid;
      int nbands = ireg[in_first]->im->Bands;
    
      int x, y;
      //float range = image_data->color.maximum - image_data->color.black;
      float range = 1;
      float min_mul = image_data->color.cam_mul[0];
      float max_mul = image_data->color.cam_mul[0];
			/*
      for(int i = 0; i < 4; i++) {
				std::cout<<"cam_mu["<<i<<"] = "<<image_data->color.cam_mul[i]<<std::endl;
      }
			*/
      for(int i = 1; i < 4; i++) {
				if( image_data->color.cam_mul[i] < min_mul )
					min_mul = image_data->color.cam_mul[i];
				if( image_data->color.cam_mul[i] > max_mul )
					max_mul = image_data->color.cam_mul[i];
      }
      //std::cout<<"range="<<range<<"  min_mul="<<min_mul<<"  new range="<<range*min_mul<<std::endl;
#ifdef RT_EMU
      /* RawTherapee emulation */
      range *= max_mul;
#else      
      range *= min_mul;
      //range *= max_mul;
#endif
    
      float mul[4] = { 
				image_data->color.cam_mul[0] * exposure / range,
				image_data->color.cam_mul[1] * exposure / range,
				image_data->color.cam_mul[2] * exposure / range,
				image_data->color.cam_mul[3] * exposure / range
      };
    
      if(r->left==0 && r->top==0) std::cout<<"RawPreprocessor::render_camwb(): nbands="<<nbands<<std::endl;
      if( nbands == 3 ) {
				float* p;
				float* pout;
				int line_sz = r->width*3;
				for( y = 0; y < r->height; y++ ) {
					p = (float*)VIPS_REGION_ADDR( ireg[in_first], r->left, r->top + y ); 
					pout = (float*)VIPS_REGION_ADDR( oreg, r->left, r->top + y ); 
					for( x=0; x < line_sz; x+=3) {
						pout[x] = p[x] * mul[0];
						pout[x+1] = p[x+1] * mul[1];
						pout[x+2] = p[x+2] * mul[2];
						//if(r->left==0 && r->top==0) std::cout<<"  p["<<x<<"]="<<p[x]<<"  pout["<<x<<"]="<<pout[x]<<std::endl;
#ifdef RT_EMU
						/* RawTherapee emulation */
						pout[x] *= 65535;
						pout[x+1] *= 65535;
						pout[x+2] *= 65535;
#endif
					}
				}
      } else {
				PF::raw_pixel_t* p;
				PF::raw_pixel_t* pout;
				for( y = 0; y < r->height; y++ ) {
					p = (PF::raw_pixel_t*)VIPS_REGION_ADDR( ireg[in_first], r->left, r->top + y ); 
					pout = (PF::raw_pixel_t*)VIPS_REGION_ADDR( oreg, r->left, r->top + y ); 
					PF::RawMatrixRow rp( p );
					PF::RawMatrixRow rpout( pout );
					for( x=0; x < r->width; x++) {
						rpout.color(x) = rp.color(x);
						rpout[x] = rp[x] * mul[ rp.color(x) ];
#ifdef RT_EMU
						/* RawTherapee emulation */
						rpout[x] *= 65535;
#endif
					}
				}
      }
    }


    void render_spotwb(VipsRegion** ireg, int n, int in_first,
											 VipsRegion* imap, VipsRegion* omap, 
											 VipsRegion* oreg, RawPreprocessorPar* par)
    {
      //dcraw_data_t* image_data = par->get_image_data();
      float exposure = par->get_exposure();
      Rect *r = &oreg->valid;
      int nbands = ireg[in_first]->im->Bands;
    
      //PF::RawPixel* p;
      //PF::RawPixel* pout;
      int x, y;
      //float range = image_data->color.maximum - image_data->color.black;
      float range = 1;
      float min_mul = par->get_wb_red();
      float max_mul = par->get_wb_red();

      if( par->get_wb_green() < min_mul )
				min_mul = par->get_wb_green();
      if( par->get_wb_green() > max_mul )
				max_mul = par->get_wb_green();

      if( par->get_wb_blue() < min_mul )
				min_mul = par->get_wb_blue();
      if( par->get_wb_blue() > max_mul )
				max_mul = par->get_wb_blue();

      //std::cout<<"range="<<range<<"  min_mul="<<min_mul<<"  new range="<<range*min_mul<<std::endl;
#ifdef RT_EMU
      /* RawTherapee emulation */
      range *= max_mul;
#else
      range *= min_mul;
#endif

      float mul[4] = { 
				par->get_wb_red() * exposure / range, 
				par->get_wb_green() * exposure / range, 
				par->get_wb_blue() * exposure / range, 
				par->get_wb_green() * exposure / range
      };
    
      if( nbands == 3 ) {
				float* p;
				float* pout;
				int line_sz = r->width*3;
				for( y = 0; y < r->height; y++ ) {
					p = (float*)VIPS_REGION_ADDR( ireg[in_first], r->left, r->top + y ); 
					pout = (float*)VIPS_REGION_ADDR( oreg, r->left, r->top + y ); 
					for( x=0; x < line_sz; x+=3) {
						pout[x] = p[x] * mul[0];
						pout[x+1] = p[x+1] * mul[1];
						pout[x+2] = p[x+2] * mul[2];
						//if(r->left==0 && r->top==0) std::cout<<"  p["<<x<<"]="<<p[x]<<"  pout["<<x<<"]="<<pout[x]<<std::endl;
#ifdef RT_EMU
						/* RawTherapee emulation */
						pout[x] *= 65535;
						pout[x+1] *= 65535;
						pout[x+2] *= 65535;
#endif
					}
				}
      } else {
				PF::raw_pixel_t* p;
				PF::raw_pixel_t* pout;
				for( y = 0; y < r->height; y++ ) {
					p = (PF::raw_pixel_t*)VIPS_REGION_ADDR( ireg[in_first], r->left, r->top + y ); 
					pout = (PF::raw_pixel_t*)VIPS_REGION_ADDR( oreg, r->left, r->top + y ); 
					PF::RawMatrixRow rp( p );
					PF::RawMatrixRow rpout( pout );
					for( x=0; x < r->width; x++) {
						rpout.color(x) = rp.color(x);
						rpout[x] = rp[x] * mul[ rp.color(x) ];
#ifdef RT_EMU
						/* RawTherapee emulation */
						rpout[x] *= 65535;
#endif
					}
				}
      }
    }


    void render(VipsRegion** ireg, int n, int in_first,
								VipsRegion* imap, VipsRegion* omap, 
								VipsRegion* oreg, OpParBase* par)
    {
      RawPreprocessorPar* rdpar = dynamic_cast<RawPreprocessorPar*>(par);
      if( !rdpar ) return;
      //std::cout<<"RawPreprocessor::render(): rdpar->get_wb_mode()="<<rdpar->get_wb_mode()<<std::endl;
      switch( rdpar->get_wb_mode() ) {
      case WB_CAMERA:
				render_camwb(ireg, n, in_first, imap, omap, oreg, rdpar);
				//std::cout<<"render_camwb() called"<<std::endl;
				break;
      case WB_SPOT:
      case WB_COLOR_SPOT:
				render_spotwb(ireg, n, in_first, imap, omap, oreg, rdpar);
				//std::cout<<"render_spotwb() called"<<std::endl;
				break;
      }
    }
  };




  ProcessorBase* new_raw_preprocessor();
}

#endif 

