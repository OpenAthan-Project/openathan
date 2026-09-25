#ifndef CALCULATIONMETHOD_HPP
#define CALCULATIONMETHOD_HPP

#include "CalculationParameters.hpp"

namespace Adhan {

namespace CalculationMethod {

// Muslim World League
CalculationParameters MuslimWorldLeague();

// Egyptian General Authority of Survey
CalculationParameters Egyptian();

// University of Islamic Sciences, Karachi
CalculationParameters Karachi();

// Umm al-Qura University, Makkah
CalculationParameters UmmAlQura();

// Dubai
CalculationParameters Dubai();

// Moonsighting Committee
CalculationParameters MoonsightingCommittee();

// ISNA
CalculationParameters NorthAmerica();

// Kuwait
CalculationParameters Kuwait();

// Qatar
CalculationParameters Qatar();

// Singapore
CalculationParameters Singapore();

// Institute of Geophysics, University of Tehran
CalculationParameters Tehran();

// Dianet
CalculationParameters Turkey();

// Other
CalculationParameters Other();

}; // namespace CalculationMethod
} // namespace Adhan

#endif /* CALCULATIONMETHOD_HPP */