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

// --------------- YAGS with 2-level local pattern table ---------------
// index bits = log2(# entries/2) = YAGS_cacheBits - 1 
// tag_bits = YAGS_ghistoryBits - index bits = YAGS_ghistoryBits - YAGS_cacheBits + 1 = 16 - 12 + 1 = 5
int YAGS_cacheBits = 14;    // Number of bits used for Cache entries   -> Cache Table: (2^YAGS_cacheBits) * (valid_bit + LRU_bit + counter_bit + tag_bit ) = (2^12) * (1+1+2+5)
int YAGS_ghistoryBits = 16; // Number of bits used for Global History 
int YAGS_lhistoryBits = 15; // Number of bits used for Local  History  -> Local  Prediction Table: (2^YAGS_lhistoryBits) * 2 = (2^12, 2)
int YAGS_pcBits = 10;       // Number of bits used for program counter -> Local  History    Table: (2^tour_pcBits) * tour_lhistoryBits = (2^10, 12)

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
uint8_t *gpt_tour;  // Global Prediction Table (2^tour_ghistoryBits) * 2 = (2^14, 2)
uint8_t *cpt_tour;  // Choice Prediction Table (2^tour_ghistoryBits) * 2 = (2^16, 2)
uint8_t *lpt_tour;  // Local  Prediction Table (2^tour_lhistoryBits) * 2 = (2^14, 2)
uint16_t *lht_tour; // Local  History    Table (2^tour_pcBits) * tour_lhistoryBits = (2^10, 14)


// --------------- YAGS with 2-level local pattern table ---------------
uint8_t *lpt_YAGS;      // Local  Prediction Table: (2^YAGS_lhistoryBits) * 2 = (2^12, 2)
uint16_t *lht_YAGS;     // Local  History    Table: (2^YAGS_pcBits) * YAGS_lhistoryBits = (2^10, 12)
// Take Cache:     (2^YAGS_cacheBits) * ( YAGS_lhistoryBits + 2) = (2^12, 1+1+2+5)
// Not Take Cache: (2^YAGS_cacheBits) * ( YAGS_lhistoryBits + 2) = (2^12, 1+1+2+5)
uint16_t *TCache_tag_YAGS;     // (2^YAGS_cacheBits)     * 5 bits
uint8_t  *TCache_counter_YAGS; // (2^YAGS_cacheBits)     * 2 bits
uint8_t  *TCache_LRU_YAGS;     // (2^YAGS_cacheBits - 1) * 1 bit

uint16_t *NTCache_tag_YAGS;     // (2^YAGS_cacheBits)     * 5 bits
uint8_t  *NTCache_counter_YAGS; // (2^YAGS_cacheBits)     * 2 bits
uint8_t  *NTCache_LRU_YAGS;     // (2^YAGS_cacheBits - 1) * 1 bit


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


// --------------- Utility function ---------------

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
  for (i = 0; i < lht_entries; i++) { lht_tour[i] = 0;  } 

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


// --------------- YAGS with 2-level local pattern table  ---------------
void init_YAGS()
{
  int cache_entries = 1 << YAGS_cacheBits;
  int lpt_entries   = 1 << YAGS_lhistoryBits;
  int lht_entries   = 1 << YAGS_pcBits;

  lpt_YAGS = (uint8_t *)malloc(lpt_entries * sizeof(uint8_t));
  lht_YAGS = (uint16_t *)malloc(lht_entries * sizeof(uint16_t));

  TCache_tag_YAGS      = (uint16_t *)malloc(cache_entries * sizeof(uint16_t));
  TCache_counter_YAGS  = (uint8_t  *)malloc(cache_entries * sizeof(uint8_t ));
  TCache_LRU_YAGS      = (uint8_t  *)malloc((cache_entries >> 1) * sizeof(uint8_t ));

  NTCache_tag_YAGS      = (uint16_t *)malloc(cache_entries * sizeof(uint16_t));
  NTCache_counter_YAGS  = (uint8_t  *)malloc(cache_entries * sizeof(uint8_t ));
  NTCache_LRU_YAGS      = (uint8_t  *)malloc((cache_entries >> 1) * sizeof(uint8_t ));

  int i = 0;
  for (i = 0; i < lpt_entries; i++) { lpt_YAGS[i] = WN; }
  for (i = 0; i < lht_entries; i++) { lht_YAGS[i] = 0;  } 

  for (i = 0; i < cache_entries; i++) { 
    TCache_tag_YAGS[i]      = 0; 
    TCache_counter_YAGS[i]  = WN;

    NTCache_tag_YAGS[i]      = 0; 
    NTCache_counter_YAGS[i]  = WN;
  }
  
  for (i = 0; i < (cache_entries >> 1); i++) { 
    TCache_LRU_YAGS[i] = 0; 
    NTCache_LRU_YAGS[i] = 0; 
  }

  ghistory = 0;
}

uint8_t YAGS_predict(uint32_t pc)
{
  uint32_t lht_entries = 1 << YAGS_pcBits;
  uint32_t lht_index = pc & (lht_entries - 1); // pc_lower_bits

  uint32_t lpt_entries = 1 << YAGS_lhistoryBits;
  uint32_t lpt_index = lht_YAGS[lht_index] & (lpt_entries-1); // lower bits of lht_YAGS[lht_index]

  // get lower ghistoryBits of pc
  uint32_t cache_entries = 1 << YAGS_ghistoryBits;
  uint32_t pc_lower_bits = pc & (cache_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (cache_entries - 1);
  uint32_t cache_index = pc_lower_bits ^ ghistory_lower_bits; // bit_num: YAGS_ghistoryBits = 16

  int set_index_bits = YAGS_cacheBits-1; // 12 - 1 = 11 bits
  uint32_t set_entries = 1 << set_index_bits; 
  uint32_t set_index = cache_index & (set_entries-1); // take LSB for 11 bits

  uint16_t tag = (cache_index >> set_index_bits); // 16 - 11 = 5 bits

  uint8_t  lpt_prediction = getPrediction(lpt_YAGS[lpt_index]);

  // pre-initiation
  uint16_t tag_0 = 0,     tag_1 = 0;
  uint8_t  counter_0 = 0, counter_1 = 0;

  switch(lpt_prediction)
  {
    case TAKEN: // check NT Cache
      // {tag_bit, counter_bit, LRU_bit, valid_bit}: {5, 2, 1, 1}
      tag_0 = NTCache_tag_YAGS[ (set_index << 1)     ];
      tag_1 = NTCache_tag_YAGS[ (set_index << 1) + 1 ];
      counter_0 =  NTCache_counter_YAGS[ (set_index << 1)     ];
      counter_1 =  NTCache_counter_YAGS[ (set_index << 1) + 1 ];
       
      if (tag == tag_0){       return getPrediction(counter_0); }
      else if (tag == tag_1){  return getPrediction(counter_1);}
      else {                              return lpt_prediction;}

    case NOTTAKEN: // check T Cache
      // {tag_bit, counter_bit, LRU_bit, valid_bit}: {5, 2, 1, 1}
      tag_0 = TCache_tag_YAGS[ (set_index << 1)     ];
      tag_1 = TCache_tag_YAGS[ (set_index << 1) + 1 ];
      counter_0 =  TCache_counter_YAGS[ (set_index << 1)     ];
      counter_1 =  TCache_counter_YAGS[ (set_index << 1) + 1 ];
       
      if (tag == tag_0){       return getPrediction(counter_0); }
      else if (tag == tag_1){  return getPrediction(counter_1);}
      else {                              return lpt_prediction;}

    default:
      printf("Warning: Undefined state of entry!\n");
      return NOTTAKEN;
  }
}

void train_YAGS(uint32_t pc, uint8_t outcome)
{
  uint32_t lht_entries = 1 << YAGS_pcBits;
  uint32_t lht_index = pc & (lht_entries - 1); // pc_lower_bits

  uint32_t lpt_entries = 1 << YAGS_lhistoryBits;
  uint32_t lpt_index = lht_YAGS[lht_index] & (lpt_entries-1);

  // Update state of entry in lpt based on outcome
  updatePredictionTableState(lpt_YAGS[lpt_index], outcome);


  // get lower ghistoryBits of pc
  uint32_t cache_entries = 1 << YAGS_ghistoryBits;
  uint32_t pc_lower_bits = pc & (cache_entries - 1);
  uint32_t ghistory_lower_bits = ghistory & (cache_entries - 1);
  uint32_t cache_index = pc_lower_bits ^ ghistory_lower_bits; // bit_num: YAGS_ghistoryBits = 16

  int set_index_bits = YAGS_cacheBits-1; // 12 - 1 = 11 bits
  uint32_t set_entries = 1 << set_index_bits; 
  uint32_t set_index = cache_index & (set_entries-1); // take LSB for 11 bits

  uint16_t tag = (cache_index >> set_index_bits); // 16 - 11 = 5 bits
  uint8_t lpt_prediction = getPrediction(lpt_YAGS[lpt_index]);

  // pre-initiation
  uint16_t tag_0 = 0,     tag_1 = 0;
  uint8_t  prediction_0 = 0, prediction_1 = 0; 

  switch (lpt_prediction)
  {
  case TAKEN:
    // {tag_bit, counter_bit, LRU_bit, valid_bit}: {5, 2, 1, 1}
    tag_0 = NTCache_tag_YAGS[ (set_index << 1)     ];
    tag_1 = NTCache_tag_YAGS[ (set_index << 1) + 1 ];
    // uint8_t  counter_0 =  NTCache_counter_YAGS[ (set_index << 1)     ];
    // uint8_t  counter_1 =  NTCache_counter_YAGS[ (set_index << 1) + 1 ];

    if (tag == tag_0){ // hit cache_data_0 -> update cache_data_0, LRU = 1
      updatePredictionTableState(NTCache_counter_YAGS[ (set_index << 1)     ], outcome);
      NTCache_LRU_YAGS[set_index] = 1;

    } else if (tag == tag_1){ // hit cache_data_1 -> update cache_data_1, LRU = 0
      updatePredictionTableState(NTCache_counter_YAGS[ (set_index << 1) + 1 ], outcome);
      NTCache_LRU_YAGS[set_index] = 0;

    } else if (outcome == NOTTAKEN){ // miss && LPT prediction is wrong


      prediction_0 = getPrediction(NTCache_counter_YAGS[ (set_index << 1) ]);
      prediction_1 = getPrediction(NTCache_counter_YAGS[ (set_index << 1) + 1 ]);

      if (prediction_0 == TAKEN) { // Redundant, update cache_data_0, LRU = 1
        NTCache_tag_YAGS[     (set_index << 1) ] = tag;
        NTCache_counter_YAGS[ (set_index << 1) ] = WN;
        NTCache_LRU_YAGS[set_index] = 1;
      } else if (prediction_1 == TAKEN) { // Redundant, update cache_data_1, LRU = 0
        NTCache_tag_YAGS[     (set_index << 1) + 1 ] = tag;
        NTCache_counter_YAGS[ (set_index << 1) + 1 ] = WN;
        NTCache_LRU_YAGS[set_index] = 0;
      } else { // Both not redundant
        if (NTCache_LRU_YAGS[set_index] == 0){ // LRU = 0 -> update cache_data_0, LRU = 1
          NTCache_tag_YAGS[     (set_index << 1) ] = tag;
          NTCache_counter_YAGS[ (set_index << 1) ] = WN;
          NTCache_LRU_YAGS[set_index] = 1;
        } else {
          NTCache_tag_YAGS[     (set_index << 1) + 1 ] = tag;
          NTCache_counter_YAGS[ (set_index << 1) + 1 ] = WN;
          NTCache_LRU_YAGS[set_index] = 0;
        }
      }
    }

    break;
  case NOTTAKEN:
    // {tag_bit, counter_bit, LRU_bit, valid_bit}: {5, 2, 1, 1}
    tag_0 = TCache_tag_YAGS[ (set_index << 1)     ];
    tag_1 = TCache_tag_YAGS[ (set_index << 1) + 1 ];
    // uint8_t  counter_0 =  TCache_counter_YAGS[ (set_index << 1)     ];
    // uint8_t  counter_1 =  TCache_counter_YAGS[ (set_index << 1) + 1 ];

    if (tag == tag_0){ // hit cache_data_0 -> update cache_data_0, LRU = 1
      updatePredictionTableState(TCache_counter_YAGS[ (set_index << 1)     ], outcome);
      TCache_LRU_YAGS[set_index] = 1;

    } else if (tag == tag_1){ // hit cache_data_1 -> update cache_data_1, LRU = 0
      updatePredictionTableState(TCache_counter_YAGS[ (set_index << 1) + 1 ], outcome);
      TCache_LRU_YAGS[set_index] = 0;

    } else if (outcome == TAKEN){ // miss && LPT prediction is wrong

      prediction_0 = getPrediction(TCache_counter_YAGS[ (set_index << 1) ]);
      prediction_1 = getPrediction(TCache_counter_YAGS[ (set_index << 1) + 1 ]);

      if (prediction_0 == NOTTAKEN) { // Redundant, update cache_data_0, LRU = 1
        TCache_tag_YAGS[     (set_index << 1) ] = tag;
        TCache_counter_YAGS[ (set_index << 1) ] = WT;
        TCache_LRU_YAGS[set_index] = 1;
      } else if (prediction_1 == NOTTAKEN) { // Redundant, update cache_data_1, LRU = 0
        TCache_tag_YAGS[     (set_index << 1) + 1 ] = tag;
        TCache_counter_YAGS[ (set_index << 1) + 1 ] = WT;
        TCache_LRU_YAGS[set_index] = 0;
      } else { // Both not redundant
        if (TCache_LRU_YAGS[set_index] == 0){ // LRU = 0 -> update cache_data_0, LRU = 1
          TCache_tag_YAGS[     (set_index << 1) ] = tag;
          TCache_counter_YAGS[ (set_index << 1) ] = WT;
          TCache_LRU_YAGS[set_index] = 1;
        } else {
          TCache_tag_YAGS[     (set_index << 1) + 1 ] = tag;
          TCache_counter_YAGS[ (set_index << 1) + 1 ] = WT;
          TCache_LRU_YAGS[set_index] = 0;
        }
      }
    }

    break;  
  default:
    printf("Warning: Undefined state of entry in YAGS LPT!\n");
    break;
  }




  // Update history register
  ghistory = ((ghistory << 1) | outcome);

  // Update state of entry in lht (local history table)
  lht_YAGS[lht_index] = ( (lht_YAGS[lht_index] << 1)   | outcome);


}

void cleanup_YAGS()
{
  free(lpt_YAGS);
  free(lht_YAGS);

  free(TCache_tag_YAGS);
  free(TCache_counter_YAGS);
  free(TCache_LRU_YAGS);

  free(NTCache_tag_YAGS);
  free(NTCache_counter_YAGS);
  free(NTCache_LRU_YAGS);
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
    init_YAGS();
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
  case CUSTOM:
    return YAGS_predict(pc);
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
      train_gshare(pc, outcome);
      return;
    case TOURNAMENT:
      train_tour(pc, outcome);
      return;
    case CUSTOM:
      train_YAGS(pc, outcome);
      return;
    default:
      break;
    }
  }
}
