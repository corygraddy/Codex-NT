-- File: sudotraffic_v11_final.lua
-- V11 FIX: Discard the first random number after seeding to avoid PRNG correlation issues.

local module = {}

-- --- USER CONFIGURATION ---
local SCALE_NOTES = {0, 4, 7} -- C, E, G

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
    self.debug_fader_cv = 0.0
    self.debug_raw_value_cv = 0.0
    self.debug_random_roll_cv = 0.0

    return {
        inputs  = 4, 
        outputs = 8
    }
end

function module.step(self, dt, inputs)
    local clock = inputs[1]
    local freeze_gate = inputs[2]
    local probability_cv_A = inputs[3]
    local probability_cv_B = inputs[4]

    self.debug_fader_cv = probability_cv_B 

    if clock > 1.0 and self.last_clock < 1.0 then
        -- (Shift Register logic...)
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
        self.debug_raw_value_cv = (raw_value / 255.0) * 5.0

        -- *** FINAL PROBABILITY LOGIC FIX ***
        
        -- 1. Seed the generator ONCE.
        math.randomseed(raw_value)

        -- 2. Prime the pump: discard the first, unreliable random number.
        local _ = math.random() 

        -- 3. Now, get the "good" random numbers for our checks.
        local roll_for_primary = math.random()
        local roll_for_harmony = math.random()
        self.debug_random_roll_cv = roll_for_harmony * 5.0

        -- 4. Primary Voice Check (now uses a reliable random number)
        local prob_thresh_A = probability_cv_A / 5.0
        if roll_for_primary < prob_thresh_A then
            self.primary_gate_out = GATE_VOLTAGE
            local note_index = (raw_value % 3) + 1
            self.primary_pitch_cv = SCALE_NOTES[note_index] * CV_PER_SEMITONE
        else
            self.primary_gate_out = 0.0
        end

        -- 5. Harmony Voice Check (now uses a reliable random number)
        local prob_thresh_B = probability_cv_B / 5.0
        if roll_for_harmony < prob_thresh_B then
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
        self.harmony_pitch_cv, self.harmony_gate_out,
        self.debug_fader_cv, self.debug_raw_value_cv,
        self.debug_random_roll_cv, 0.0 
    }
end

return module