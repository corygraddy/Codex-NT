-- File: SuperToolbox_v7.lua
-- Author: [Your Name/Alias]
-- Version: 7.0
-- Description: A consolidated multi-mode, multi-output utility algorithm for the Disting NT.
-- See "SuperToolbox_Guide.txt" for full documentation.

local module = {}

function module.init(self)
    self.mode = 0
    -- State variables for stateful modes
    self.vactrol_last_cv = 0.0
    self.ducking_env = 0.0
    self.ping_env = 0.0
    self.last_clock = 0.0
    self.router_state = 1
    self.lfo_phases = {0, 0.25, 0.5, 0.75}
    self.seq_step = 0
    self.pendulum_step = 0
    self.pendulum_dir = 1

    return {
        inputs  = 4,
        outputs = 4,
        parameters = {
            {
                name = "Mode",
                min = 0, max = 35, default = 0,
                string_values = {
                    "Vactrol-Style VCA", "Deluxe Attenuverter", "Crossfader",
                    "Stochastic AND", "Stochastic OR", "Stochastic XOR",
                    "Prob. Trigger", "Gate Skipper", "Comparator", "Voltage Clamp",
                    "Hard Clipper", "Half-Wave Rect.", "Full-Wave Rect.", "Bipolar->Unipolar",
                    "Octave Switch", "Semitone Trans.", "Chaotic Shaper", "Freq Divider",
                    "--- COMBINATIONS ---",
                    "Window Comparator",
                    "--- MACRO FUNCTIONS ---",
                    "Side-Chain Ducker", "Chaotic Trigger Gen", "Pinged LPG Sim", "Folded LFO",
                    "--- MULTI-OUTPUT ---",
                    "Chaos Router", "Quad LFO", "Dual Prob. Trigger",
                    "Sequencer (UP)", "Sequencer (DOWN)", "Sequencer (UP/DOWN)",
                    "Sequencer (DOWN/UP)", "Sequencer (INWARD)", "Sequencer (OUTWARD)",
                    "Stepped Pendulum"
                }
            }
        }
    }
end

function module.step(self, dt, inputs)
    local results = {0.0, 0.0, 0.0, 0.0}
    local current_mode = self.mode

    -- --- SINGLE-OUTPUT MODES ---
    if current_mode <= 24 then
        local single_result = 0.0

        if current_mode == 0 then -- Vactrol-Style VCA
            local slew_amount = 1.0 - (math.max(0, math.min(5.0, inputs[3])) / 5.0 * 0.99)
            local slewed_cv = (inputs[2] * slew_amount) + (self.vactrol_last_cv * (1.0 - slew_amount))
            self.vactrol_last_cv = slewed_cv
            single_result = inputs[1] * (slewed_cv / 5.0)
        elseif current_mode == 1 then -- Deluxe Attenuverter
            local curve = (inputs[3] / 5.0) * 4.0 - 2.0
            local scaled_cv = math.pow(inputs[2] / 5.0, math.pow(2, curve)) * 5.0
            single_result = inputs[1] * (scaled_cv / 2.5 - 1.0)
        elseif current_mode == 2 then -- Crossfader
            single_result = (inputs[1] * (1 - (inputs[3] / 5.0))) + (inputs[2] * (inputs[3] / 5.0))
        elseif current_mode == 3 then -- Stochastic AND
            single_result = (inputs[1] > 1.0 and inputs[2] > 1.0 and math.random() < (inputs[3] / 5.0)) and 5.0 or 0.0
        elseif current_mode == 4 then -- Stochastic OR
            single_result = ((inputs[1] > 1.0 or inputs[2] > 1.0) and math.random() < (inputs[3] / 5.0)) and 5.0 or 0.0
        elseif current_mode == 5 then -- Stochastic XOR
            single_result = (((inputs[1] > 1.0) ~= (inputs[2] > 1.0)) and math.random() < (inputs[3] / 5.0)) and 5.0 or 0.0
        elseif current_mode == 6 then -- Prob. Trigger
            single_result = (inputs[1] > 1.0 and math.random() < (inputs[2] / 5.0)) and 5.0 or 0.0
        elseif current_mode == 7 then -- Gate Skipper
            single_result = (inputs[1] > 1.0 and math.random() > (inputs[2] / 5.0)) and 5.0 or 0.0
        elseif current_mode == 8 then -- Comparator
            single_result = (inputs[1] > inputs[2]) and 5.0 or 0.0
        elseif current_mode == 9 then -- Voltage Clamp
            single_result = math.max(inputs[2], math.min(inputs[3], inputs[1]))
        elseif current_mode == 10 then -- Hard Clipper
            single_result = math.max(-5.0, math.min(5.0, inputs[1] * (inputs[2] / 5.0 * 4)))
        elseif current_mode == 11 then -- Half-Wave Rectifier
            single_result = math.max(0, inputs[1])
        elseif current_mode == 12 then -- Full-Wave Rectifier
            single_result = math.abs(inputs[1])
        elseif current_mode == 13 then -- Bipolar->Unipolar
            single_result = inputs[1] + 5.0
        elseif current_mode == 14 then -- Octave Switch
            single_result = inputs[1] + (inputs[2] > 1.0 and 1.0 or 0.0)
        elseif current_mode == 15 then -- Semitone Transposer
            single_result = inputs[1] + (math.floor(inputs[2] / 5.0 * 12) * (1/12))
        elseif current_mode == 16 then -- Chaotic Shaper
            single_result = math.sin(inputs[1] * 10) * math.cos(inputs[1] * 3.14) * 5.0
        elseif current_mode == 17 then -- Simple Freq Divider
            single_result = (inputs[1] > 0 and (math.floor(inputs[1]*1000) % 2 == 0)) and 2.5 or -2.5
        elseif current_mode == 19 then -- Window Comparator
            single_result = (inputs[1] > inputs[2] and inputs[1] < inputs[3]) and 5.0 or 0.0
        elseif current_mode == 21 then -- Side-Chain Ducker
            if inputs[2] > 1.0 then self.ducking_env = 1.0 end
            self.ducking_env = self.ducking_env * 0.995
            local depth = inputs[3] / 5.0
            single_result = inputs[1] * (1.0 - (self.ducking_env * depth))
        elseif current_mode == 22 then -- Chaotic Trigger Gen
            local chaos_val = math.sin(inputs[1] * 10) * math.cos(inputs[1] * 3.14) * 5.0
            single_result = (chaos_val > inputs[2]) and 5.0 or 0.0
        elseif current_mode == 23 then -- Pinged LPG Sim
            if inputs[2] > 1.0 and self.last_clock < 1.0 then self.ping_env = 1.0 end
            local decay_rate = 1.0 - (math.max(0.01, inputs[3] / 5.0) * 0.1)
            self.ping_env = self.ping_env * decay_rate
            single_result = inputs[1] * self.ping_env
            self.last_clock = inputs[2]
        elseif current_mode == 24 then -- Folded LFO
            local drive = 1.0 + (inputs[2] / 5.0 * 4.0)
            single_result = math.abs(inputs[1] * drive) - (drive / 2.0)
        end
        results[1] = single_result

    -- --- MULTI-OUTPUT MODES ---
    elseif current_mode == 26 then -- Chaos Router
        local clock = inputs[4]
        if clock > 1.0 and self.last_clock < 1.0 then self.router_state = (self.router_state % 3) + 1 end
        self.last_clock = clock
        if self.router_state == 1 then results[1], results[2], results[3] = inputs[1], inputs[2], inputs[3]
        elseif self.router_state == 2 then results[1], results[2], results[3] = inputs[2], inputs[3], inputs[1]
        elseif self.router_state == 3 then results[1], results[2], results[3] = inputs[3], inputs[1], inputs[2] end
        results[4] = (results[1] + results[3]) * 0.5

    elseif current_mode == 27 then -- Quad LFO
        local rate = inputs[1] / 5.0 * 10.0
        local shape_cv = inputs[2] / 5.0
        for i = 1, 4 do
            self.lfo_phases[i] = (self.lfo_phases[i] + dt * rate) % 1.0
            local saw = (self.lfo_phases[i] * 2.0) - 1.0
            local sine = math.sin(self.lfo_phases[i] * 2.0 * 3.14159)
            results[i] = ((saw * shape_cv) + (sine * (1.0 - shape_cv))) * 5.0
        end

    elseif current_mode == 28 then -- Dual Prob. Trigger
        local clock = inputs[1]
        if clock > 1.0 and self.last_clock < 1.0 then
            if math.random() < (inputs[2] / 5.0) then results[1] = 5.0 else results[1] = 0.0 end
            if math.random() < (inputs[3] / 5.0) then results[2] = 5.0 else results[2] = 0.0 end
        elseif clock < 1.0 and self.last_clock > 1.0 then
            results[1], results[2] = 0.0, 0.0
        end
        self.last_clock = clock

    elseif current_mode >= 29 and current_mode <= 34 then -- Sequencer Modes
        local clock = inputs[1]
        local reset = inputs[2]
        local range_cv = inputs[3]
        local cv_val = 0.0
        if reset > 1.0 then self.seq_step = 0 end
        if clock > 1.0 and self.last_clock < 1.0 then
            results[2] = 5.0
            local steps = (current_mode == 31 or current_mode == 32) and 14 or 8
            self.seq_step = (self.seq_step + 1) % steps
        elseif clock < 1.0 and self.last_clock > 1.0 then
            results[2] = 0.0
        end
        if current_mode == 29 then local s = {0,1,2,3,4,5,6,7}; cv_val = s[self.seq_step+1]/7.0
        elseif current_mode == 30 then local s = {7,6,5,4,3,2,1,0}; cv_val = s[self.seq_step+1]/7.0
        elseif current_mode == 31 then local s = {0,1,2,3,4,5,6,7,6,5,4,3,2,1}; cv_val = s[self.seq_step+1]/7.0
        elseif current_mode == 32 then local s = {7,6,5,4,3,2,1,0,1,2,3,4,5,6}; cv_val = s[self.seq_step+1]/7.0
        elseif current_mode == 33 then local s = {7,0,6,1,5,2,4,3}; cv_val = s[self.seq_step+1]/7.0
        elseif current_mode == 34 then local s = {3,4,2,5,1,6,0,7}; cv_val = s[self.seq_step+1]/7.0 end
        results[1] = cv_val * (range_cv / 5.0)
        self.last_clock = clock

    elseif current_mode == 35 then -- Stepped Pendulum
        local clock = inputs[1]
        local min_cv = inputs[2]
        local max_cv = inputs[3]
        local steps_cv = inputs[4]
        local total_steps = math.max(2, math.floor(steps_cv / 5.0 * 62) + 2)
        results[2] = 0.0
        if clock > 1.0 and self.last_clock < 1.0 then
            self.pendulum_step = self.pendulum_step + self.pendulum_dir
            if self.pendulum_step >= total_steps - 1 then
                self.pendulum_step = total_steps - 1; self.pendulum_dir = -1; results[2] = 5.0
            elseif self.pendulum_step <= 0 then
                self.pendulum_step = 0; self.pendulum_dir = 1; results[2] = 5.0
            end
        end
        self.last_clock = clock
        local position = self.pendulum_step / (total_steps - 1)
        results[1] = (position * (max_cv - min_cv)) + min_cv
    end
    
    return results
end

return module