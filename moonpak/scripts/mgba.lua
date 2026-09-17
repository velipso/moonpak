-- SPDX-License-Identifier: 0BSD
--
-- This script will display CPU usage percentage overlayed on the mGBA screen in the upper-right.
--   Top number - last frame CPU usage
--   Bottom number - max CPU usage across 120 frames
--
-- It is designed to be added as an autorun script in the UI:
--   Tools -> Scripting
--   File -> Edit autorun scripts
--

local layer
local painter
local digits = {
  [" "] = {"000"},
  ["0"] = {"011","101","101","101","110"},
  ["1"] = {"010","110","010","010","111"},
  ["2"] = {"110","001","010","100","111"},
  ["3"] = {"111","001","010","001","110"},
  ["4"] = {"101","101","111","001","001"},
  ["5"] = {"111","100","111","001","110"},
  ["6"] = {"011","100","111","101","110"},
  ["7"] = {"111","001","010","010","010"},
  ["8"] = {"011","101","010","101","110"},
  ["9"] = {"011","101","111","001","110"},
  ["."] = {"0","0","0","0","1"},
  ["%"] = {"101","001","010","100","101"},
}

function drawChar(ch, x, y, color)
  local glyph = digits[ch]
  if not glyph then return -1 end
  local width = 0
  for row = 1, #glyph do
    width = math.max(width, #glyph[row])
    for col = 1, #glyph[row] do
      if glyph[row]:sub(col, col) == "1" then
        layer.image:setPixel(x + col - 1, y + row - 1, color)
      end
    end
  end
  return width
end

function drawString(s, x, y, color)
  for i = 1, #s do
    x = x + drawChar(s:sub(i, i), x, y, color) + 1
  end
end

function drawNum(n, tx, ty)
  local txt = string.format("%.1f%%", n)
  while #txt < 6 do
    txt = " " .. txt
  end
  for y = 0,2 do
    for x = 0,2 do
      drawString(txt, tx + x, ty + y, 0x77000000)
    end
  end
  drawString(txt, tx + 1, ty + 1, 0x77ffffff)
end

local frameStart = 0
local frameEnded = false
function onFrameStart()
  if frameEnded then
    frameStart = emu:currentCycle()
    frameEnded = false
  end
end

local percents = {}
for i=1,120 do percents[i] = 0 end
local percentsWrite = 1
function onFrameEnd()
  frameEnded = true
  local used = emu:currentCycle() - frameStart
  if used < 0 then return end
  local percent = used * 100 / 280896
  if percent > 999.9 then percent = 999.9 end

  percents[percentsWrite] = percent
  percentsWrite = math.fmod(percentsWrite, #percents) + 1
  maxPercent = 0
  for _, v in pairs(percents) do
    maxPercent = math.max(maxPercent, v)
  end

  painter:setFillColor(0)
  painter:drawRectangle(0, 0, 23, 15)
  drawNum(percent, 0, 0)
  drawNum(maxPercent, 0, 8)
  layer:update()
end

function onSwi()
  local code = emu:read16(emu:readRegister("lr") - 2)
  if (code & 0x0f00) == 0x0f00 then -- works for both ARM and Thumb
    code = code & 0xff
  else -- not an SWI call I guess
    return
  end

  if code == 5 then
    onFrameEnd()
  elseif code == 4 then
    local flags = emu:readRegister("r1")
    if (flags & 1) ~= 0 then -- VBlank IRQ
      onFrameEnd()
    end
  end
end

function onStart()
  layer = canvas:newLayer(23, 15)
  layer:setPosition(216, 1)

  painter = image.newPainter(layer.image)
  painter:setBlend(false)
  painter:setFill(true)
  painter:setStrokeWidth(0)

  callbacks:add("frame", onFrameStart)
  emu:setBreakpoint(onSwi, 0x00000008)
end

callbacks:add("start", onStart)
