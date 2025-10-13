-- File: input_test.lua
-- PURPOSE: A simple hardware diagnostic tool.
-- It routes the CV from Inputs X, Y, Z, and W directly to 
-- Outputs A, B, C, and D respectively.

local module = {}

function module.init(self)
    return {
        inputs  = 4,
        outputs = 4
    }
end

function module.step(self, dt, inputs)
    -- Simply pass each input directly to its corresponding output
    local out_a = inputs[1] -- Voltage from Input X
    local out_b = inputs[2] -- Voltage from Input Y
    local out_c = inputs[3] -- Voltage from Input Z (Primary Prob Fader)
    local out_d = inputs[4] -- Voltage from Input W (Harmony Prob Fader)
    
    return { out_a, out_b, out_c, out_d }
end

return module