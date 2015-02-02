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

//#include <arpa/inet.h>

#include "raw_output.hh"

/* We need C linkage for this.
 */
#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

#include "../dt/common/colorspaces.h"

#ifdef __cplusplus
}
#endif /*__cplusplus*/

//#include "../dt/external/adobe_coeff.c"
//#include "../vips/vips_layer.h"


PF::RawOutputPar::RawOutputPar(): 
  OpParBase(),
  image_data( NULL ),
  profile_mode("profile_mode",this,PF::IN_PROF_MATRIX,"MATRIX","MATRIX"),
  //profile_mode("profile_mode",this,PF::IN_PROF_NONE,"NONE","NONE"),
  current_profile_mode( IN_PROF_MATRIX ),
  gamma_curve( NULL ),
  cam_profile_name("cam_profile_name", this),
  cam_profile( NULL ),
  gamma_mode("gamma_mode",this,PF::IN_GAMMA_NONE,"NONE","None"),
  gamma_lin("gamma_lin", this, 0),
  gamma_exp("gamma_exp", this, 2.2),
  out_profile_mode("out_profile_mode",this,PF::OUT_PROF_sRGB,"sRGB","Built-in sRGB"),
  current_out_profile_mode( OUT_PROF_sRGB ),
  out_profile_name("out_profile_name", this),
  out_profile( NULL ),
  transform( NULL )
{
  profile_mode.add_enum_value(PF::IN_PROF_NONE,"NONE","NONE");
  profile_mode.add_enum_value(PF::IN_PROF_MATRIX,"MATRIX","MATRIX");
  profile_mode.add_enum_value(PF::IN_PROF_ICC,"ICC","ICC");

  out_profile_mode.add_enum_value(PF::OUT_PROF_NONE,"NONE","NONE");
  //out_profile_mode.add_enum_value(PF::OUT_PROF_sRGB,"sRGB","Built-in sRGB");
  out_profile_mode.add_enum_value(PF::OUT_PROF_ADOBE,"ADOBE","Built-in Adobe RGB 1998");
  out_profile_mode.add_enum_value(PF::OUT_PROF_PROPHOTO,"PROPHOTO","Built-in ProPhoto RGB");
  out_profile_mode.add_enum_value(PF::OUT_PROF_LAB,"LAB","Lab");
  out_profile_mode.add_enum_value(PF::OUT_PROF_CUSTOM,"CUSTOM","Custom");

  gamma_mode.add_enum_value(PF::IN_GAMMA_NONE,"NONE","None");
  gamma_mode.add_enum_value(PF::IN_GAMMA_sRGB,"sRGB","sRGB");
  gamma_mode.add_enum_value(PF::IN_GAMMA_CUSTOM,"CUSTOM","Custom");

  cmsToneCurve* curve = Build_sRGBGamma(NULL);
  srgb_curve = cmsReverseToneCurve( curve );
  cmsFreeToneCurve( curve );

  set_type("raw_output" );
}


VipsImage* PF::RawOutputPar::build(std::vector<VipsImage*>& in, int first, 
				     VipsImage* imap, VipsImage* omap, 
				     unsigned int& level)
{
  if( in.size() < first+1 )
    return NULL;

  VipsImage* image = in[first];
  if( !image )
    return NULL;

  size_t blobsz;
  if( vips_image_get_blob( in[0], "raw_image_data",
			   (void**)&image_data, 
			   &blobsz ) )
    return NULL;
  if( blobsz != sizeof(dcraw_data_t) )
    return NULL;

  bool mode_changed = profile_mode.is_modified();
  bool out_mode_changed = out_profile_mode.is_modified();
  bool gamma_mode_changed = gamma_mode.is_modified();
  bool cam_changed = cam_profile_name.is_modified();
  bool out_changed = out_profile_name.is_modified();
  /*
  if( profile_mode.get_enum_value().first != (int)current_profile_mode )
    mode_changed = true;
  if( out_profile_mode.get_enum_value().first != (int)current_out_profile_mode )
    out_mode_changed = true;
  if( cam_profile_name.get() != current_cam_profile_name )
    cam_changed = true;
  if( out_profile_name.get() != current_out_profile_name )
    out_changed = true;

  current_profile_mode = (input_profile_mode_t)profile_mode.get_enum_value().first;
  current_out_profile_mode = (output_profile_mode_t)out_profile_mode.get_enum_value().first;
  current_cam_profile_name = cam_profile_name.get();
  current_out_profile_name = out_profile_name.get();
  */

  //std::cout<<"RawOutputPar::build(): mode_changed="<<mode_changed
  //         <<"  out_mode_changed="<<out_mode_changed
  //         <<"  cam_changed="<<cam_changed
  //         <<"  out_changed="<<out_changed<<std::endl;

  bool changed = mode_changed || out_mode_changed || gamma_mode_changed || cam_changed || out_changed || out_mode_changed ||
    (cam_profile==NULL) || (out_profile==NULL);

  if( cam_profile && (mode_changed || cam_changed) ) {
    cmsCloseProfile( cam_profile );
    cam_profile = NULL;
  }

  // create input camera profile based on Adobe matrices
  if( mode_changed || (cam_profile == NULL) ) {

    switch( profile_mode.get_enum_value().first ) {
    case PF::IN_PROF_MATRIX:
      cam_profile = dt_colorspaces_create_xyzimatrix_profile((float (*)[3])image_data->color.cam_xyz);
      break;
    case PF::IN_PROF_ICC:
      if( !cam_profile_name.get().empty() )
        cam_profile = cmsOpenProfileFromFile( cam_profile_name.get().c_str(), "r" );
      break;
    default:
      break;
    }
  } else if( (profile_mode.get_enum_value().first == PF::IN_PROF_ICC) && cam_changed ) {
    cam_profile = cmsOpenProfileFromFile( cam_profile_name.get().c_str(), "r" );
  }

  if( out_profile && (out_mode_changed || out_changed) ) {
    cmsCloseProfile( out_profile );
    out_profile = NULL;
  }

  if( out_mode_changed || out_changed || (out_profile == NULL) ) {
    //std::cout<<"RawOutputPar::build(): out_mode_changed="<<out_mode_changed
    //         <<"  out_changed="<<out_changed<<"  out_profile="<<out_profile<<std::endl;
    //std::cout<<"  out_profile_mode="<<out_profile_mode.get_enum_value().first<<std::endl;
    switch( out_profile_mode.get_enum_value().first ) {
    case OUT_PROF_sRGB:
      out_profile = dt_colorspaces_create_srgb_profile();
      //std::cout<<"RawOutputPar::build(): created sRGB output profile"<<std::endl;
      break;
    case OUT_PROF_ADOBE:
      out_profile = dt_colorspaces_create_adobergb_profile();
      //std::cout<<"RawOutputPar::build(): created AdobeRGB output profile"<<std::endl;
      break;
    case OUT_PROF_PROPHOTO:
      out_profile = dt_colorspaces_create_prophotorgb_profile();
      //std::cout<<"RawOutputPar::build(): created ProPhoto output profile"<<std::endl;
      break;
    case OUT_PROF_LAB:
      out_profile = dt_colorspaces_create_lab_profile();
      //std::cout<<"RawOutputPar::build(): created Lab output profile"<<std::endl;
      break;
    case OUT_PROF_CUSTOM:
      //std::cout<<"  custom profile selected: \""<<cam_profile_name.get()<<"\""<<std::endl;
      if( !out_profile_name.get().empty() )
        out_profile = cmsOpenProfileFromFile( out_profile_name.get().c_str(), "r" );
      break;
    default:
      break;
    }
  }

  if( changed ) {
    if( transform )
      cmsDeleteTransform( transform );  
    transform = NULL;
    if( cam_profile && out_profile )
      transform = cmsCreateTransform( cam_profile, 
				      TYPE_RGB_FLT,
				      out_profile, 
				      TYPE_RGB_FLT,
				      INTENT_PERCEPTUAL, 
				      cmsFLAGS_NOCACHE );
  }
  std::cout<<"RawOutputPar::build(): transform="<<transform<<std::endl;

  if( gamma_curve )
    cmsFreeToneCurve( gamma_curve );
  float gamma = gamma_exp.get();
  gamma_curve = cmsBuildGamma( NULL, 1.0f/gamma );

  VipsImage* rotated = image;
  switch( image_data->sizes.flip ) {
  case 6:
    if( vips_rot(image, &rotated, VIPS_ANGLE_D90, NULL) )
      return NULL;
    break;
  case 3:
    if( vips_rot(image, &rotated, VIPS_ANGLE_D180, NULL) )
      return NULL;
    break;
  case 5:
    if( vips_rot(image, &rotated, VIPS_ANGLE_D270, NULL) )
      return NULL;
    break;
  default: 
    PF_REF( rotated, "RawOutputPar::build(): rotated ref" );
    break;
  }
  if( !rotated ) return NULL;
  set_image_hints( rotated );

  std::vector<VipsImage*> in2;
  in2.push_back( rotated );

  VipsImage* out = OpParBase::build( in2, first, NULL, NULL, level );
  if( out ) {
    PF_UNREF( rotated, "RawOutputPar::build(): rotated unref" );
  }
  /**/
  if( out_profile ) {
    cmsUInt32Number out_length;
    cmsSaveProfileToMem( out_profile, NULL, &out_length);
    void* buf = malloc( out_length );
    cmsSaveProfileToMem( out_profile, buf, &out_length);
    vips_image_set_blob( out, VIPS_META_ICC_NAME, 
			 (VipsCallbackFn) g_free, buf, out_length );
    char tstr[1024];
    cmsGetProfileInfoASCII(out_profile, cmsInfoDescription, "en", "US", tstr, 1024);
    std::cout<<"RawOutputPar::build(): image="<<out<<"  embedded profile: "<<tstr<<std::endl;
  }
  /**/
  return out;
}



PF::ProcessorBase* PF::new_raw_output()
{
  return new PF::Processor<PF::RawOutputPar,PF::RawOutput>();
}
