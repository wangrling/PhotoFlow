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

#ifndef TRANSFER_COLORS_CONFIG_DIALOG_HH
#define TRANSFER_COLORS_CONFIG_DIALOG_HH

#include <gtkmm.h>

#include "../../operation_config_gui.hh"

#include "../../widgets/layerlist.hh"


namespace PF {

class GmicTransferColorsConfigGUI: public OperationConfigGUI
{
  Gtk::Button updateButton;
  LayerList layer_list;
  Slider regularizationSlider;
  Slider lumiSlider;
  Selector precisionSelector;

public:
  GmicTransferColorsConfigGUI( Layer* l );

  void on_layer_changed();

  void init();

  void on_update();
  void do_update();
};

}

#endif