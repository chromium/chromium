This directory contains <area> tests run with the HTMLAreaElementDisplayNone
runtime feature disabled, i.e. with the kill switch for making <area>
display:none by default flipped. It exists to make sure that flipping the kill
switch restores the old display:inline behavior everywhere, so the baselines
here are the pre-crbug.com/1231263 results.
