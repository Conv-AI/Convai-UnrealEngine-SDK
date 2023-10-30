// Copyright 2022 Convai Inc. All Rights Reserved.


#include "ConvaiDefinitions.h"

const TMap<EEmotionIntensity, float> FConvaiEmotionState::ScoreMultipliers = 
{
	{EEmotionIntensity::None, 0.0},
	{EEmotionIntensity::LessIntense, 0.25},
	{EEmotionIntensity::Basic, 0.6},
	{EEmotionIntensity::MoreIntense, 1}
};