#include <adhan/CalculationMethod.hpp>

namespace Adhan {

// Muslim World League
CalculationParameters CalculationMethod::MuslimWorldLeague() {
  auto params = CalculationParameters("MuslimWorldLeague", 18, 17);
  params.methodAdjustments.dhuhr = 1;
  return params;
}

// Egyptian General Authority of Survey
CalculationParameters CalculationMethod::Egyptian() {
  auto params = CalculationParameters("Egyptian", 19.5, 17.5);
  params.methodAdjustments.dhuhr = 1;
  return params;
}

// University of Islamic Sciences, Karachi
CalculationParameters CalculationMethod::Karachi() {
  auto params = CalculationParameters("Karachi", 18, 18);
  params.methodAdjustments.dhuhr = 1;
  return params;
}

// Umm al-Qura University, Makkah
CalculationParameters CalculationMethod::UmmAlQura() {
  return {"UmmAlQura", 18.5, 0, 90};
}

// Dubai
CalculationParameters CalculationMethod::Dubai() {
  auto params = CalculationParameters("Dubai", 18.2, 18.2);
  params.methodAdjustments.sunrise = -3;
  params.methodAdjustments.dhuhr = 3;
  params.methodAdjustments.asr = 3;
  params.methodAdjustments.maghrib = 3;

  return params;
}

// Moonsighting Committee
CalculationParameters CalculationMethod::MoonsightingCommittee() {
  auto params = CalculationParameters("MoonsightingCommittee", 18, 18);
  params.methodAdjustments.dhuhr = 5;
  params.methodAdjustments.maghrib = 3;
  return params;
}

// ISNA
CalculationParameters CalculationMethod::NorthAmerica() {
  auto params = CalculationParameters("NorthAmerica", 15, 15);
  params.methodAdjustments.dhuhr = 1;
  return params;
}

// Kuwait
CalculationParameters CalculationMethod::Kuwait() {
  return {"Kuwait", 18, 17.5};
}

// Qatar
CalculationParameters CalculationMethod::Qatar() {
  return {"Qatar", 18, 0, 90};
}

// Singapore
CalculationParameters CalculationMethod::Singapore() {
  auto params = CalculationParameters("Singapore", 20, 18);
  params.methodAdjustments.dhuhr = 1;
  params.rounding = Rounding::Up;
  return params;
}

// Institute of Geophysics, University of Tehran
CalculationParameters CalculationMethod::Tehran() {
  auto params = CalculationParameters("Tehran", 17.7, 14, 0, 4.5);
  return params;
}

// Dianet
CalculationParameters CalculationMethod::Turkey() {
  auto params = CalculationParameters("Turkey", 18, 17);
  params.methodAdjustments.sunrise = -7;
  params.methodAdjustments.dhuhr = 5;
  params.methodAdjustments.asr = 4;
  params.methodAdjustments.maghrib = 7;
  return params;
}

// Other
CalculationParameters CalculationMethod::Other() {
  return {"Other", 0, 0};
}
} // namespace Adhan