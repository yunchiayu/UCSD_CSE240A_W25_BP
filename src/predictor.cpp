//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "YunChiaYu";
const char *studentID = "A69032045";
const char *email = "yuy088@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = {"Static", "Gshare", "Tournament", "Custom"};

// define number of bits required for indexing the BHT here.
int ghistoryBits = 17; // Number of bits used for Global History
int bpType;            // Branch Prediction Type
int verbose;

// --------------- Tournament ---------------
int tour_choiceBits   = 14; // Number of bits used for Choice Table    -> Choice Prediction Table has 2^12 entries
int tour_ghistoryBits = 16; // Number of bits used for Global History  -> Global Prediction Table has 2^12 entries
int tour_lhistoryBits = 14; // Number of bits used for Local  History  -> Local  Prediction Table has 2^10 entries
int tour_pcBits = 10;       // Number of bits used for program counter -> Local  History    Table : (2^tour_pcBits) * tour_lhistoryBits = (2^10, 10)




//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
// TODO: Add your own Branch Predictor data structures here
//

uint64_t ghistory;
// --------------- gshare ---------------
uint8_t *bht_gshare;

// --------------- tournament ---------------
uint8_t *gpt_tour;  // Global Prediction Table (2^tour_ghistoryBits) * 2 = (2^12, 2)
uint8_t *cpt_tour;  // Choice Prediction Table (2^tour_ghistoryBits) * 2 = (2^12, 2)
uint8_t *lpt_tour;  // Local  Prediction Table (2^tour_lhistoryBits) * 2 = (2^10, 2)
uint16_t *lht_tour; // Local  History    Table (2^tour_pcBits) * tour_lhistoryBits = (2^10, 10)


//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//

// --------------- gshare functions ---------------
void init_gshare()
{
  int bht_entries = 1 << ghistoryBits;
  bht_gshare = (uint8_t *)malloc(bht_entries * sizeof(uint8_t));
  int i = 0;
  for (i = 0; i < bht_entries; i++)
  {
    bht_gshare[i] = WN;
  }
  ghistory = 0;
}

uint8_t gshare_predict(uint32_t pc)
{
  // get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  switch (bht_gshare[index])
  {
  case WN:
    return NOTTAKEN;
  case SN:
    return NOTTAKEN;
  case WT:
    return TAKEN;
  case ST:
    return TAKEN;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    return NOTTAKEN;
  }
}

void train_gshare(uint32_t pc, uint8_t outcome)
{
  // get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

  // Update state of entry in bht based on outcome
  switch (bht_gshare[index])
  {
  case WN:
    bht_gshare[index] = (outcome == TAKEN) ? WT : SN;
    break;
  case SN:
    bht_gshare[index] = (outcome == TAKEN) ? WN : SN;
    break;
  case WT:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WN;
    break;
  case ST:
    bht_gshare[index] = (outcome == TAKEN) ? ST : WT;
    break;
  default:
    printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    break;
  }

  // Update history register
  ghistory = ((ghistory << 1) | outcome);
}

void cleanup_gshare()
{
  free(bht_gshare);
}


// --------------- Prediction function ---------------

uint8_t getPrediction(uint8_t state) {
    switch (state)
    {
      case WN:
        return NOTTAKEN;
      case SN:
        return NOTTAKEN;
      case WT:
        return TAKEN;
      case ST:
        return TAKEN;
      default:
        printf("Warning: Undefined state of entry!\n");
        return NOTTAKEN;
    }
}

void updatePredictionTableState(uint8_t &state, uint8_t outcome) {
  switch (state) {
    case SN:
      state = (outcome == TAKEN) ? WN : SN;
      break;
    case WN:
      state = (outcome == TAKEN) ? WT : SN;
      break;
    case WT:
      state = (outcome == TAKEN) ? ST : WN;
      break;
    case ST:
      state = (outcome == TAKEN) ? ST : WT;
      break;
    default:
      printf("Warning: Undefined state of entry!\n");
      break;
  }
}

// --------------- tournament ---------------
void init_tour()
{
  int gpt_entries = 1 << tour_ghistoryBits;
  int cpt_entries = 1 << tour_choiceBits;  
  int lpt_entries = 1 << tour_lhistoryBits;
  int lht_entries = 1 << tour_pcBits;

  gpt_tour = (uint8_t *)malloc(gpt_entries * sizeof(uint8_t));
  cpt_tour = (uint8_t *)malloc(cpt_entries * sizeof(uint8_t));
  lpt_tour = (uint8_t *)malloc(lpt_entries * sizeof(uint8_t));
  lht_tour = (uint16_t *)malloc(lpt_entries * sizeof(uint16_t));

  int i = 0;
  for (i = 0; i < gpt_entries; i++) { gpt_tour[i] = WN; }
  for (i = 0; i < cpt_entries; i++) { cpt_tour[i] = WL; }
  for (i = 0; i < lpt_entries; i++) { lpt_tour[i] = WN; }
  for (i = 0; i < lht_entries; i++) { lpt_tour[i] = 0;  } 

  ghistory = 0;
}

uint8_t tour_predict(uint32_t pc)
{
  // get lower tour_ghistoryBits of ghistory
  uint32_t gpt_entries = 1 << tour_ghistoryBits;
  uint32_t gpt_index = ghistory & (gpt_entries - 1); // ghistory_lower_bits

  uint32_t cpt_entries = 1 << tour_choiceBits;
  uint32_t cpt_index = ghistory & (cpt_entries - 1); // ghistory_lower_bits

  uint32_t lht_entries = 1 << tour_pcBits;
  uint32_t lht_index = pc & (lht_entries - 1); // pc_lower_bits

  uint32_t lpt_entries = 1 << tour_lhistoryBits;
  uint32_t lpt_index = lht_tour[lht_index] & (lpt_entries-1);

  switch(cpt_tour[cpt_index])
  {
    case SL:
      return getPrediction(lpt_tour[lpt_index]);
    case WL:
      return getPrediction(lpt_tour[lpt_index]);
    case WG:
      return getPrediction(gpt_tour[gpt_index]);
    case SG:
      return getPrediction(gpt_tour[gpt_index]);  
    default:
      printf("Warning: Undefined state of entry in Tournament CPT!\n");
      return NOTTAKEN;
  }
}

void train_tour(uint32_t pc, uint8_t outcome)
{
  // get lower tour_ghistoryBits of ghistory
  uint32_t gpt_entries = 1 << tour_ghistoryBits;
  uint32_t gpt_index = ghistory & (gpt_entries - 1); // ghistory_lower_bits

  uint32_t cpt_entries = 1 << tour_choiceBits;
  uint32_t cpt_index = ghistory & (cpt_entries - 1); // ghistory_lower_bits

  uint32_t lht_entries = 1 << tour_pcBits;
  uint32_t lht_index = pc & (lht_entries - 1); // pc_lower_bits

  uint32_t lpt_entries = 1 << tour_lhistoryBits;
  uint32_t lpt_index = lht_tour[lht_index] & (lpt_entries-1);

  // Update state of entry in gpt based on outcome
  updatePredictionTableState(gpt_tour[gpt_index], outcome);

  // Update state of entry in lpt based on outcome
  updatePredictionTableState(lpt_tour[lpt_index], outcome);

  // Update state of entry in cpt based on outcome (Only update when local & global predictor disagree each other)
  uint8_t gpt_prediction = getPrediction(gpt_tour[gpt_index]);
  uint8_t lpt_prediction = getPrediction(lpt_tour[lpt_index]);
  if (gpt_prediction != lpt_prediction) 
  {
    switch (cpt_tour[cpt_index])
    {
      case SL:
        cpt_tour[cpt_index] = (outcome == gpt_prediction) ? WL : SL;
        break;
      case WL:
        cpt_tour[cpt_index] = (outcome == gpt_prediction) ? WG : SL;
        break;
      case WG:
        cpt_tour[cpt_index] = (outcome == gpt_prediction) ? SG : WL;
        break;
      case SG:
        cpt_tour[cpt_index] = (outcome == gpt_prediction) ? SG : WG;
        break;
      default:
        printf("Warning: Undefined state of entry in Tournament CPT!\n");
        break;
    }
  }


  // Update history register
  ghistory = ((ghistory << 1) | outcome);

  // Update state of entry in lht (local history table)
  lht_tour[lht_index] = ( (lht_tour[lht_index] << 1)   | outcome);

}

void cleanup_tour()
{
  free(gpt_tour);
  free(cpt_tour);
  free(lpt_tour);
  free(lht_tour);
}


// ============================================================

void init_predictor()
{
  switch (bpType)
  {
  case STATIC:
    break;
  case GSHARE:
    init_gshare();
    break;
  case TOURNAMENT:
    init_tour();
    break;
  case CUSTOM:
    break;
  default:
    break;
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint32_t make_prediction(uint32_t pc, uint32_t target, uint32_t direct)
{

  // Make a prediction based on the bpType
  switch (bpType)
  {
  case STATIC:
    return TAKEN;
  case GSHARE:
    return gshare_predict(pc);
  case TOURNAMENT:
    return tour_predict(pc);
    // return NOTTAKEN;
  case CUSTOM:
    return NOTTAKEN;
  default:
    break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void train_predictor(uint32_t pc, uint32_t target, uint32_t outcome, uint32_t condition, uint32_t call, uint32_t ret, uint32_t direct)
{
  if (condition)
  {
    switch (bpType)
    {
    case STATIC:
      return;
    case GSHARE:
      return train_gshare(pc, outcome);
    case TOURNAMENT:
      return train_tour(pc, outcome);
      return;
    case CUSTOM:
      return;
    default:
      break;
    }
  }
}
