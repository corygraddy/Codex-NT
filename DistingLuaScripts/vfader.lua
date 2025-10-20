-- vfader.lua (minimal): I2C in -> 64 virtual faders (parameters), 14-bit MIDI CC out

return {
    init = function(self)
        -- Layout
        self.NUM_PAGES = 8
        self.PER_PAGE = 8
        self.TOTAL = self.NUM_PAGES * self.PER_PAGE -- 64

        -- UI state
        self.page = 1
        self.sel = 1 -- 1..64

        -- Build minimal parameter list:
        -- 1) MIDI channel (1..16)
        -- 2..65) 64 fader params (normalized 0..1, 0.001 steps)
        local params = {
            { "MIDI channel", 1, 16, 1, kNone },
        }
        self._faderBase = #params -- index before appending, so first fader will be base+1
        for i = 1, self.TOTAL do
            table.insert(params, { string.format("Fader %02d", i), 0, 1000, 0, kNone, kBy1000 })
        end

        -- Track last values to detect changes originated by mappings (MIDI/I2C)
    self._last = {}
    for i = 1, self.TOTAL do self._last[i] = 0.0 end
    -- Track if a fader has ever been assigned/changed (for UI '-/-' vs 'msb/lsb')
    self._everSet = {}
    for i = 1, self.TOTAL do self._everSet[i] = false end
    -- Throttled MIDI send queue
    self._pendingQ = {}
    self._pendingInQ = {}
    self._sendBudgetPerStep = 8

        return {
            name = "VFADER (I2C→14-bit CC)",
            description = "Map I2C to 64 params; sends 14-bit MIDI CC pairs on change.",
            parameters = params,
            midi = { channelParameter = 1, messages = { "cc" } },
        }
    end,

    -- Poll for parameter changes and emit 14-bit CC pairs. No audio/CV outputs.
    step = function(self, dt, inputs)
        local chan = self.parameters[1] or 1
        if chan < 1 or chan > 16 then return end
        local status = 0xB0 + ((chan - 1) & 0x0F) -- avoid bitops; simple add is fine here
        local where = 4 -- USB only (kept minimal to avoid spamming other ports)
        local base = self._faderBase or 1
        -- Detect changes and enqueue indices (deduplicated)
        for i = 1, self.TOTAL do
            local pIdx = base + i
            local v = self.parameters[pIdx] or 0.0 -- 0..1 via kBy1000
            if v < 0 then v = 0 elseif v > 1 then v = 1 end
            if math.abs(v - (self._last[i] or -1)) > 0.0005 then
                self._last[i] = v
                self._everSet[i] = true
                if not self._pendingInQ[i] then
                    table.insert(self._pendingQ, i)
                    self._pendingInQ[i] = true
                end
            end
        end
        -- Send up to budget CC pairs per step
        local budget = self._sendBudgetPerStep or 8
        while budget > 0 and #self._pendingQ > 0 do
            local i = table.remove(self._pendingQ, 1)
            self._pendingInQ[i] = nil
            local v = self._last[i] or 0.0
            local full = math.floor(v * 16383 + 0.5)
            local msb = math.floor(full / 128)
            local lsb = full % 128
            local msbCC, lsbCC
            if i <= 32 then
                msbCC = (i - 1)
                lsbCC = 32 + (i - 1)
            else
                msbCC = 64 + (i - 33)
                lsbCC = 96 + (i - 33)
            end
            sendMIDI(where, status, msbCC, msb)
            sendMIDI(where, status, lsbCC, lsb)
            budget = budget - 1
        end
    end,

    -- Required to receive encoder/pot events
    ui = function(self)
        return true
    end,

    -- Minimal UI: page + eight values; enc1=page, enc2=select, pot2 adjusts selected (optional helper)
    draw = function(self)
        local drawText = _G.drawText
        local drawTinyText = _G.drawTinyText
        local drawRectangle = _G.drawRectangle
        local drawBox = _G.drawBox
        local drawLine = _G.drawLine
        local base = self._faderBase or 1

        -- Header: show selected fader index and its 14-bit CC pair
        local sel = self.sel or 1
        local msbCC, lsbCC
        if sel <= 32 then
            msbCC = (sel - 1)
            lsbCC = 32 + (sel - 1)
        else
            msbCC = 64 + (sel - 33)
            lsbCC = 96 + (sel - 33)
        end
        local header = string.format("F%02d  CC %d/%d (14b)  Pg %d/%d  Ch %d", sel, msbCC, lsbCC, self.page, self.NUM_PAGES, self.parameters[1] or 1)
        drawText(8, 8, header, 15)

        -- Page indicators
        local indicator_height = 8
        local indicator_width = 32
        local y_offset = 14
        for i = 1, self.NUM_PAGES do
            local x1 = (i - 1) * indicator_width
            local x2 = x1 + indicator_width - 3
            if i == self.page then
                drawRectangle(x1, y_offset, x2, indicator_height + y_offset, 15)
            else
                drawBox(x1, y_offset, x2, indicator_height + y_offset, 7)
            end
        end

        -- Names & numbers layout
        local fader_base_index = (self.page - 1) * self.PER_PAGE
        local col_width = 32
        local y_odd_names = 32
        local y_even_names = 44
        local y_numbers = 58
        local nameShift = { 8, 8, 5, 3, -3, -5, -8, -10 }

        -- Current page local selection and neighbors for triple underline
        local localSel = ((sel - 1) % self.PER_PAGE) + 1
        local leftLocal = (localSel > 1) and (localSel - 1) or 1
        local rightLocal = (localSel < self.PER_PAGE) and (localSel + 1) or self.PER_PAGE

        for i = 1, self.PER_PAGE do
            local idx = fader_base_index + i
            local col_start_x = (i - 1) * col_width
            local col_end_x = i * col_width
            local x_center = col_start_x + (col_width / 2)

            local v = self.parameters[base + idx] or 0.0
            if v < 0 then v = 0 elseif v > 1 then v = 1 end
            local valueStr = tostring(math.floor(v * 100 + 0.5))
            local nameStr = string.format("FADR %02d", idx)

            local shift = nameShift[i] or 0
            drawText(x_center + shift, y_numbers, valueStr, 15, "centre")

            local y_pos = (i % 2 == 1) and y_odd_names or y_even_names
            local isSelected = (i == localSel)
            local nameColour = isSelected and 15 or 7
            if isSelected then
                local maxChars = 6
                local big = nameStr
                if #big > maxChars then big = big:sub(1, maxChars) end
                local x_text = col_start_x + (col_width / 2) + shift
                drawText(x_text, y_pos + 3, big, 15, "centre")
            else
                if i <= 4 then
                    drawTinyText(col_start_x + 2 + shift, y_pos, nameStr, nameColour)
                else
                    drawTinyText(col_end_x - 2 + shift, y_pos, nameStr, nameColour, "right")
                end
            end

            -- Linking tick between name & number for odd columns
            if (i % 2) == 1 then
                local x_line = x_center + shift
                local y_mid = math.floor((y_pos + y_numbers) * 0.5 + 0.5) - 3 -- moved down by 2px
                local halfLen = 3
                drawLine(x_line, y_mid - halfLen, x_line, y_mid + halfLen, 6)
            end

            -- CC labels removed to reduce clutter and prevent overlap
        end

        -- Triple underlines under left/selected/right faders at the numbers row
        do
            local function centerXFor(iCol)
                local csx = (iCol - 1) * col_width + (col_width / 2)
                return csx + (nameShift[iCol] or 0)
            end
            local yUL = y_numbers + 2
            if yUL <= 63 then
                for _, iCol in ipairs({ leftLocal, localSel, rightLocal }) do
                    local cx = centerXFor(iCol)
                    local xL = cx - 12
                    local xR = cx + 12
                    if xL < 0 then xL = 0 end
                    if xR > 255 then xR = 255 end
                    drawLine(xL, yUL, xR, yUL, (iCol == localSel) and 15 or 10)
                end
            end
        end

        return true
    end,

    encoder1Turn = function(self, delta)
        local p = (self.page or 1) + (delta or 0)
        if p < 1 then p = self.NUM_PAGES elseif p > self.NUM_PAGES then p = 1 end
        self.page = p
        -- keep selection within page
        local localIdx = ((self.sel - 1) % self.PER_PAGE) + 1
        self.sel = (self.page - 1) * self.PER_PAGE + localIdx
    end,

    encoder2Turn = function(self, delta)
        local localIdx = ((self.sel - 1) % self.PER_PAGE) + 1
        localIdx = ((localIdx - 1 + (delta or 0)) % self.PER_PAGE) + 1
        self.sel = (self.page - 1) * self.PER_PAGE + localIdx
    end,

    -- Wire the top three pots to left/selected/right faders within the current page
    pot1Turn = function(self, value)
        value = math.max(0.0, math.min(1.0, value or 0.0))
        local base = self._faderBase or 1
        local localSel = ((self.sel - 1) % self.PER_PAGE) + 1
        local leftLocal = (localSel > 1) and (localSel - 1) or 1
        local idx = (self.page - 1) * self.PER_PAGE + leftLocal
        setParameterNormalized(self.algorithmIndex, self.parameterOffset + base + idx, value, false)
    end,

    -- Optional: allow tweaking selected fader locally (good for testing without I2C)
    pot2Turn = function(self, value)
        value = math.max(0.0, math.min(1.0, value or 0.0))
        local base = self._faderBase or 1
        local pIdx = base + (self.sel or 1)
        -- Use setParameterNormalized so mappings/automation are consistent
        setParameterNormalized(self.algorithmIndex, self.parameterOffset + pIdx, value, false)
    end,

    pot3Turn = function(self, value)
        value = math.max(0.0, math.min(1.0, value or 0.0))
        local base = self._faderBase or 1
        local localSel = ((self.sel - 1) % self.PER_PAGE) + 1
        local rightLocal = (localSel < self.PER_PAGE) and (localSel + 1) or self.PER_PAGE
        local idx = (self.page - 1) * self.PER_PAGE + rightLocal
        setParameterNormalized(self.algorithmIndex, self.parameterOffset + base + idx, value, false)
    end,
}