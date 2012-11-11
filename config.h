/*
Open Diversity - Video based diversity software
Idea by Rangarid
Hardware design by Daniel
Implementation by Rangarid

Copyright 2011-2012 by Rangarid

This file is part of Open Diversity

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. */

//####Debug mode#####
//comment if not used
#define DEBUG

// Start channel
int channel = 1;

// Time to refresh LCD data
int lcdrefresh = 500;

// Startscreen
int screen = 1;

// Beep on source Switch
int beeponswitch = 1;

// Voltage threshold for low voltage alarm
float lowvoltage = 9.9;

// Amplification factor for Videoquality
int kfps = 10;

// Amplification factor for RSSI
int krssi = 1;

// Diversity Sensitivity for switching Hysteresis
int sensitivity = 50;


