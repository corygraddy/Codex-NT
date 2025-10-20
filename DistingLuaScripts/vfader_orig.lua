-- This script acts as a Hybrid Sequencer/Controller Manager.
-- LAST LOAD ID: 251017_2300EDT (Final Drawing Fix - Manual Function Definition)
-- It has been refactored to encapsulate ALL constants within the self table for robust initialization.

local drawText = _G.drawText
local drawTinyText = _G.drawTinyText
local drawLine = _G.drawLine
local setTextSize = _G.setTextSize

return {
    -- --- Initialization: Defines I/O and State ---
    init = function(self)
        -- --- CORE CONSTANTS (Now defined on self for guaranteed initialization) ---
        self.NUM_PAGES = 8
        self.NUM_FADERS = 8
        self.TOTAL_VIRTUAL_FADERS = self.NUM_PAGES * self.NUM_FADERS 
        
        self.PAGE_FLIP_INPUT = 1       
        self.CLOCK_INPUT = 2           
        self.DIRECTION_INPUT = 3       
        self.SEQ_LENGTH_CONTROL = 13   

        self.CLOCK_THRESHOLD = 2.5     
        self.FADER_CC_START = 0        
        self.TAKEOVER_THRESHOLD = 0.02 
        self.MOD_CC_OUT_BUS = 3        
        self.PITCH_CV_OUT = 1
        self.GATE_OUT = 2

        -- --- STATE AND CONFIGURATION ---
        self.CurrentPage = 1
        self.LastPageInputVal = 0.0 
        
        -- State Management for Sequencing 
        self.SequenceLength = 1 -- Default to 1 step
        self.CurrentGlobalStep = 0 -- 0-based index (0 to 63)
        self.LastClockValue = 0.0
        self.CurrentDirection = 0 -- 0=Forward
        self.PingPongDirection = 1 -- 1 for fwd, -1 for rev

        -- Assignments, Fader Values, and Takeover Arrays
        self.Assignments = {}          
        self.FaderValues = {}          
        self.FaderTakeover = {}        
        
        -- UI Data
        self.PageNames = {
            "PAGE 1: FADERS 00-07", "PAGE 2: FADERS 08-15", "PAGE 3: FADERS 16-23", "PAGE 4: FADERS 24-31",
            "PAGE 5: FADERS 32-39", "PAGE 6: FADERS 40-47", "PAGE 7: FADERS 48-55", "PAGE 8: FADERS 56-63"
        }
        self.DirectionNames = { "FWD", "REV", "P-PONG", "RAND", "DRUNK", "SKIP 2", "SKIP 3", "SKIP 4" }

        
        -- Initialize the Assignments table (64 virtual faders) with sequential CCs (0 to 63)
        for p = 1, self.NUM_PAGES do
            self.Assignments[p] = {}
            for f = 1, self.NUM_FADERS do
                local cc = (p - 1) * self.NUM_FADERS + (f - 1)
                self.Assignments[p][f] = {
                    name = string.format("CC%02d", cc),
                    cc_number = cc,
                }
                self.FaderValues[(p - 1) * self.NUM_FADERS + f] = 0.0
            end
        end
        
        -- Initialize Takeover Flags to TRUE
        for i = 1, self.NUM_FADERS do
            self.FaderTakeover[i] = true
        end
        
        -- --- Final I/O Definition ---
        return {
            name = "F8R Hybrid Sequencer",
            description = "64-Step Matrix + 8-Fader CC Controller",
            inputs = {
                { name = "Page Flip CV", min = 0.0, max = 1.0 },
                { name = "Clock In", min = 0.0, max = 5.0, units = kNT_unitVolts, type = kGate },
                { name = "Direction CV", min = 0.0, max = 10.0, units = kNT_unitVolts }, 
                { name = "Unused Mod CV", min = 0.0, max = 10.0, units = kNT_unitVolts }, 
                
                -- MIDI CC 0-7 Input (Placeholder inputs defined in the API for MIDI CCs)
                { name = "MIDI CC 0 (F1)", min = 0.0, max = 0.0 }, { name = "MIDI CC 1 (F2)", min = 0.0, max = 0.0 }, 
                { name = "MIDI CC 2 (F3)", min = 0.0, max = 0.0 }, { name = "MIDI CC 3 (F4)", min = 0.0, max = 0.0 }, 
                { name = "MIDI CC 4 (F5)", min = 0.0, max = 0.0 }, { name = "MIDI CC 5 (F6)", min = 0.0, max = 0.0 }, 
                { name = "MIDI CC 6 (F7)", min = 0.0, max = 0.0 }, { name = "MIDI CC 7 (F8)", min = 0.0, max = 0.0 }, 
                
                { name = "Seq Length (1-64)", min = 0.0, max = 10.0, units = kNT_unitVolts },
            },
            outputs = {
                { name = "Pitch CV", min = 0.0, max = 10.0, units = kNT_unitVolts, type = kLinear },
                { name = "Gate Out", min = 0.0, max = 5.0, units = kNT_unitVolts, type = kStepped },
                { name = "Mod CC Out", min = 0.0, max = 10.0, units = kNT_unitVolts, type = kLinear }, 
            },
            parameters = {}
        }
    end,

    -- Helper function to calculate the next step index
    getNextStep = function(self)
        local direction_val = self.inputs[self.DIRECTION_INPUT].value
        self.CurrentDirection = math.floor(direction_val * (8 - 0.01)) -- 0-7 for 8 modes
        
        local next_step = self.CurrentGlobalStep

        if self.CurrentDirection == 0 then -- Forward
            next_step = self.CurrentGlobalStep + 1
        elseif self.CurrentDirection == 1 then -- Reverse
            next_step = self.CurrentGlobalStep - 1
        elseif self.CurrentDirection == 2 then -- Ping-Pong
            next_step = self.CurrentGlobalStep + self.PingPongDirection
            if next_step >= self.SequenceLength - 1 or next_step <= 0 then
                self.PingPongDirection = -self.PingPongDirection
            end
        elseif self.CurrentDirection == 3 then -- Random
            next_step = math.random(0, self.SequenceLength - 1)
        elseif self.CurrentDirection == 4 then -- Drunk
            next_step = self.CurrentGlobalStep + math.random(-1, 1)
        elseif self.CurrentDirection == 5 then -- Skip 2
            next_step = self.CurrentGlobalStep + 2
        elseif self.CurrentDirection == 6 then -- Skip 3
            next_step = self.CurrentGlobalStep + 3
        elseif self.CurrentDirection == 7 then -- Skip 4
            next_step = self.CurrentGlobalStep + 4
        end

        -- Wrap the step within the defined sequence length (0-based)
        if next_step >= self.SequenceLength then
            next_step = next_step - self.SequenceLength
        elseif next_step < 0 then
            next_step = next_step + self.SequenceLength
        end

        self.CurrentGlobalStep = next_step
        return next_step
    end,

    -- --- Main Step Loop: Reads Faders and Handles Hybrid Logic ---
    step = function(self)
        -- *** CRITICAL SAFETY CHECK ***: ONLY execute if core I/O is present
        if not self.inputs or not self.outputs then
            return -- Exit immediately if I/O is not ready
        end

        -- --- 1. SET SEQUENCE LENGTH ---
        -- Read Seq Length CV (Input 13) and scale 0-10V to 1-64 steps
        local seq_length_cv = self.inputs[self.SEQ_LENGTH_CONTROL].value
        self.SequenceLength = math.max(1, math.floor(seq_length_cv * 6.3) + 1) -- Scales 0-10V to 1-64

        -- --- 2. CLOCK DETECTION AND SEQUENCING ---
        local clock_in_val = self.inputs[self.CLOCK_INPUT].value
        local trigger_detected = clock_in_val >= self.CLOCK_THRESHOLD and self.LastClockValue < self.CLOCK_THRESHOLD
        self.LastClockValue = clock_in_val
        
        local current_step_value = 0.0
        local is_sequencing_active = false
        
        if trigger_detected then
            -- Advance step using the helper function
            local global_step_index = self:getNextStep()
            
            -- Get the value from the virtual fader corresponding to the current step
            current_step_value = self.FaderValues[global_step_index + 1] or 0.0
            
            -- Output the CV and Gate for the step
            self.outputs[self.PITCH_CV_OUT].value = current_step_value * 10.0  -- Scale 0-1 to 0-10V
            self.outputs[self.GATE_OUT].value = 5.0 -- 5V Gate pulse
            
            is_sequencing_active = true
        else
            -- Ensure the Gate output drops immediately if no trigger is detected
            self.outputs[self.GATE_OUT].value = 0.0
        end

        -- --- 3. HANDLE PAGE FLIP (Same Logic as before) ---
        local page_input_val = self.inputs[self.PAGE_FLIP_INPUT].value
        local new_page = math.floor(page_input_val * (self.NUM_PAGES - 0.01)) + 1
        
        if new_page ~= self.CurrentPage then
            self.CurrentPage = new_page
            for i = 1, self.NUM_FADERS do
                self.FaderTakeover[i] = true -- Re-engage takeover on page change
            end
        end

        -- --- 4. HYBRID FADER / MODULATION LOGIC ---
        for f = 1, self.NUM_FADERS do
            local physical_cc_id = self.FADER_CC_START + f - 1 
            local current_fader_val_0_1 = receiveMidiCc(physical_cc_id) 
            local virtual_fader_index_1based = (self.CurrentPage - 1) * self.NUM_FADERS + f 

            -- Store the live value
            self.FaderValues[virtual_fader_index_1based] = current_fader_val_0_1
            
            -- Determine if this Fader is currently a STEP SOURCE
            -- The virtual fader index (0-63) is checked against the sequence length (1-64)
            local is_step_source = virtual_fader_index_1based <= self.SequenceLength

            if is_step_source then
                -- **FADER IS IN SEQUENCE MODE: DO NOT SEND MIDI CC**
                -- The step sequencer handles this output (Output 1 and 2)
                -- Optional: Use the physical fader's value to temporarily override the running step's value
                if self.inputs[f + 4] and self.FaderValues[virtual_fader_index_1based] then -- Check if physical CV fader is patched
                   -- self.outputs[self.PITCH_CV_OUT].value = current_fader_val_0_1 * 10.0 
                   -- We skip CV output here to keep the sequencing smooth, but we still read the fader for the next step's value
                end
            else
                -- **FADER IS IN MIDI CC MODE**
                local param = self.Assignments[self.CurrentPage][f]
                
                -- --- Takeover Logic Check ---
                if self.FaderTakeover[f] then
                    local stored_val = self.FaderValues[virtual_fader_index_1based]
                    local delta = math.abs(current_fader_val_0_1 - stored_val)
                    
                    if delta <= self.TAKEOVER_THRESHOLD then
                        self.FaderTakeover[f] = false -- Takeover complete
                    end
                    
                    -- If locked, do nothing (output remains frozen/stored value)
                    -- If unlocked, send the live value and modulation CV
                end

                if not self.FaderTakeover[f] then
                    -- Send MIDI CC (Standard Fader Mode)
                    sendMidiCc(param.cc_number, current_fader_val_0_1)
                    
                    -- Send Modulation CV (Output 3)
                    self.outputs[self.MOD_CC_OUT_BUS].value = current_fader_val_0_1 * 10.0
                else
                    -- Keep the Mod CV Output quiet if the fader is locked/not in use
                    self.outputs[self.MOD_CC_OUT_BUS].value = 0.0
                end
            end
        end
    end,

    -- --- Custom UI Drawing ---
    draw = function(self)
        -- *** CRITICAL SAFETY CHECK ***: ONLY execute if core I/O is present
        if not self.inputs or not self.outputs then
            return -- Exit immediately if I/O is not ready
        end
        
        -- Enforce drawing function scope
        local dt = _G.drawTinyText
        local dw = _G.drawText
        local dl = _G.drawLine
        local st = _G.setTextSize

        st(1)
        
        local virtual_fader_base_index_0based = (self.CurrentPage - 1) * self.NUM_FADERS
        local seq_steps_text = string.format("Steps: %d/64", self.SequenceLength)
        local current_step_text = string.format("Step: %d", self.CurrentGlobalStep + 1)
        local mode_text = "FADER MODE"
        if self.SequenceLength > 0 then
            mode_text = "SEQ ACTIVE"
        end
        local direction_text = self.DirectionNames[self.CurrentDirection + 1]

        -- Draw Header (Top Line)
        local header_y = 5 -- Shifted down from 0
        dw(0, header_y, self.PageNames[self.CurrentPage], 15) -- White
        dw(100, header_y, mode_text, 12) -- Status Text (Green-ish)
        dt(150, header_y, direction_text, 12) -- Direction Text
        dt(190, header_y, seq_steps_text, 12) -- Seq Steps (Yellow-ish)
        dt(230, header_y, current_step_text, 12) -- Current Step (Yellow-ish)

        -- --- Draw Fader Values (Row 2) and Names (Row 3) ---
        for f = 1, self.NUM_FADERS do
            local virtual_fader_index_1based = virtual_fader_base_index_0based + f
            local x_pos = (f - 1) * 32 
            
            local value_to_display = self.FaderValues[virtual_fader_index_1based] or 0.0
            local is_step_source = virtual_fader_index_1based <= self.SequenceLength
            local is_takeover = self.FaderTakeover[f]

            -- 1. Draw Percentage Level (Row 2)
            local pct = math.floor(value_to_display * 100)
            local display_text = string.format("%d%%", pct)
            local color = 15 -- Default White (Max Brightness)

            if is_step_source and self.SequenceLength > 0 then
                color = 12 -- Bright Green for Active Step Source
                if virtual_fader_index_1based == self.CurrentGlobalStep + 1 then
                    color = 15 -- Full White for Current Step
                end
            elseif is_takeover then
                color = 6 -- Dim Red/Gray for Locked/Takeover
                display_text = "[LOCKED]"
            end
            
            dw(x_pos, 15, display_text, color) -- Shifted from 10 to 15

            -- 2. Draw Fader Name (Row 3)
            local param = self.Assignments[self.CurrentPage][f]
            local name = param.name or "CC"
            local status_indicator = is_step_source and "STEP" or "CC"
            
            dt(x_pos, 25, string.format("%s (%s)", name, status_indicator), 7) -- Darker Grey (Shifted from 20 to 25)
        end
        
        -- Draw simple visible line to confirm draw loop completion
        dl(0, 63, 255, 63, 15)
        
        return true -- Suppress the standard parameter line
    end,

    encoder1Turn = function(self, x)
        self.CurrentPage = self.CurrentPage + x
        if self.CurrentPage > self.NUM_PAGES then
            self.CurrentPage = 1
        elseif self.CurrentPage < 1 then
            self.CurrentPage = self.NUM_PAGES
        end
        for i = 1, self.NUM_FADERS do
            self.FaderTakeover[i] = true -- Re-engage takeover on page change
        end
    end,

    encoder2Turn = function(self, x)
        self.SequenceLength = self.SequenceLength + x
        if self.SequenceLength > 64 then
            self.SequenceLength = 64
        elseif self.SequenceLength < 1 then
            self.SequenceLength = 1
        end
    end,

    ui = function(self)
        return true
    end,
}
