#
#  This file is part of Permafrost Engine. 
#  Copyright (C) 2019 Eduard Permyakov 
#
#  Permafrost Engine is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  Permafrost Engine is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
#  Linking this software statically or dynamically with other modules is making 
#  a combined work based on this software. Thus, the terms and conditions of 
#  the GNU General Public License cover the whole combination. 
#  
#  As a special exception, the copyright holders of Permafrost Engine give 
#  you permission to link Permafrost Engine with independent modules to produce 
#  an executable, regardless of the license terms of these independent 
#  modules, and to copy and distribute the resulting executable under 
#  terms of your choice, provided that you also meet, for each linked 
#  independent module, the terms and conditions of the license of that 
#  module. An independent module is a module which is not derived from 
#  or based on Permafrost Engine. If you modify Permafrost Engine, you may 
#  extend this exception to your version of Permafrost Engine, but you are not 
#  obliged to do so. If you do not wish to do so, delete this exception 
#  statement from your version.
#

import pf
from constants import *
import view_controller

class TabBarVC(view_controller.ViewController):

    def __init__(self, view):
        self.view = view
        self.active_idx = 0
        self.labels = []
        self.children = []

    def __on_tab_changed(self, event):
        assert self.active_idx >= 0 and self.active_idx < len(self.children)
        assert event >= 0 and event < len(self.children)
        self.children[self.active_idx].deactivate()
        self.active_idx = event
        self.children[self.active_idx].activate()

    def push_child(self, label, vc):
        assert isinstance(vc, view_controller.ViewController)
        assert isinstance(label, basestring)
        self.children.append(vc)
        self.view.push_child(label, vc.view)

    def activate(self):
        pf.register_event_handler(EVENT_SETTINGS_TAB_SEL_CHANGED, TabBarVC.__on_tab_changed, self)
        self.children[self.active_idx].activate()
        self.view.show()

    def deactivate(self):
        self.view.hide()
        self.children[self.active_idx].deactivate()
        pf.unregister_event_handler(EVENT_SETTINGS_TAB_SEL_CHANGED, TabBarVC.__on_tab_changed)

