-- File: GenerativeHarmonySequencer.lua
-- Author: [Your Name/Alias]
-- Version: 1.0 (based on the final working sudotraffic script)
-- Description: A generative duophonic sequencer for the Disting NT.
-- Creates a 3-note, duophonic sequence from a clocked, feedback-driven shift register with probabilistic gates.
-- Originally intended as a drum pattern generator to use with the Poly Multi-Sample. You configure the 
-- SCALE_NOTES array to the notes of the drum sounds you want to use.
-- See README.md for full documentation.

local module = {}

-- --- USER CONFIGURATION ---
local SCALE_NOTES = {0, 4, 7} -- C, E, G (in semitones from C)

-- Constants
local GATE_VOLTAGE = 5.0
local CV_PER_SEMITONE = 1.0 / 12.0
local REGISTER_LENGTH = 8

function module.init(self)
    self.register = {0, 0, 0, 0, 0, 0, 0, 1}
    self.last_clock = 0
    self.primary_pitch_cv = 0.0
    self.primary_gate_out = 0.0
    self.harmony_pitch_cv = 0.0
    self.harmony_gate_out = 0.0

    return {
        inputs  = 4, 
        outputs = 4
    }
end

function module.step(self, dt, inputs)
    local clock = inputs[1]
    local freeze_gate = inputs[2]
    local probability_cv_A = inputs[3]
    local probability_cv_B = inputs[4]

    if clock > 1.0 and self.last_clock < 1.0 then
        -- Shift Register Logic
        if freeze_gate < 1.0 then
            local new_bit = (self.register[1] ~= self.register[REGISTER_LENGTH]) and 1 or 0
            for i = REGISTER_LENGTH, 2, -1 do self.register[i] = self.register[i-1] end
            self.register[1] = new_bit
        else
            local last_bit = self.register[REGISTER_LENGTH]
            for i = REGISTER_LENGTH, 2, -1 do self.register[i] = self.register[i-1] end
            self.register[1] = last_bit
        end

        local raw_value = 0
        for i = 1, REGISTER_LENGTH do raw_value = raw_value + self.register[i] * (2^(i-1)) end
        
        -- Seeding and Random Rolls
        math.randomseed(raw_value)
        local _ = math.random() -- Prime the pump
        local roll_for_primary = math.random()
        local roll_for_harmony = math.random()

        -- Primary Voice Logic
        if roll_for_primary < (probability_cv_A / 5.0) then
            self.primary_gate_out = GATE_VOLTAGE
            local note_index = (raw_value % 3) + 1
            self.primary_pitch_cv = SCALE_NOTES[note_index] * CV_PER_SEMITONE
        else
            self.primary_gate_out = 0.0
        end

        -- Harmony Voice Logic
        if roll_for_harmony < (probability_cv_B / 5.0) then
            self.harmony_gate_out = GATE_VOLTAGE
            local primary_note_index = (raw_value % 3) + 1
            local harmony_options = {}
            for i=1, #SCALE_NOTES do
                if i ~= primary_note_index then table.insert(harmony_options, SCALE_NOTES[i]) end
            end
            local harmony_semitone = harmony_options[math.random(1, #harmony_options)]
            self.harmony_pitch_cv = harmony_semitone * CV_PER_SEMITONE
        else
            self.harmony_gate_out = 0.0
            self.harmony_pitch_cv = 0.0
        end

    elseif clock < 1.0 and self.last_clock > 1.0 then
        self.primary_gate_out = 0.0
        self.harmony_gate_out = 0.0
    end
    
    self.last_clock = clock

    return { 
        self.primary_pitch_cv, self.primary_gate_out, 
        self.harmony_pitch_cv, self.harmony_gate_out
    }
end

return module
