//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "NAME";
const char *studentID   = "PID";
const char *email       = "EMAIL";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
//TODO: Add your own Branch Predictor data structures here
//

// Not static, use malloc
int* branchHistoryTable;
uint32_t globalHistory;
uint32_t globalMask;


int* localPredcitionTable;
uint32_t* localHistoryTable;
uint32_t localHistory;
uint32_t localMask;
int* chooserTable;
uint32_t pcMask;


#define PCSIZE 600
#define PERCEPTRON_HEIGHT 30
#define PERCEPTRON_SATUATELEN 8
#define PERCEPTRON_MASK_PC(x) ((x * 19) % PCSIZE)

int16_t perceptron[PCSIZE][PERCEPTRON_HEIGHT + 1];
int16_t perceptronGHistory[PERCEPTRON_HEIGHT];
int32_t perceptronTrainTheta;
uint8_t perceptronRecentPrediction = NOTTAKEN;
uint8_t perceptronNeedTrain = 0;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
void
init_predictor()
{
  //
  //TODO: Initialize Branch Predictor Data Structures
  //
  switch(bpType) {
    case STATIC:
      break;
    case GSHARE:
      init_gshare_predictor();
      break;
    case TOURNAMENT:
      init_tournament_predictor();
      break;
    case CUSTOM:
      init_perceptron();
      break;
    default:
      break;
  }
}

uint32_t
construct_mask(int size) {
  uint32_t mask = (1 << size) - 1;
  return mask;
}

// Initialize the GSHARE predictor
//
void
init_gshare_predictor() {
  //
  //TODO: Initialize GSHARE Branch Predictor Data Structures
  //
  branchHistoryTable = init_prediction_table(ghistoryBits, WN);
  globalHistory = 0;
  globalMask = construct_mask(ghistoryBits);
}

void
train_gshare(uint32_t pc, uint8_t res) {
  uint32_t index = (globalHistory ^ pc) & globalMask;
  update_prediction_table(branchHistoryTable, index, res);
  update_history(&globalHistory, res);
}

void
train_tournament(uint32_t pc, uint8_t res) {
  int whichPredictorToTake = chooserTable[globalHistory & globalMask];
  uint8_t globalRes = two_bit_predictor(branchHistoryTable[globalHistory & globalMask]);
  uint8_t localRes = two_bit_predictor(localPredcitionTable[localHistoryTable[pc & pcMask] & localMask]);
  bool globalCorrect = (globalRes == res);
  bool localCorrect = (localRes == res);
  int delta = (int)globalCorrect - (int)localCorrect;
  if(delta == 1) {
    // global res is correct, while local res is incorrect
    if(whichPredictorToTake != ST) {
      chooserTable[globalHistory & globalMask]++;
    }
  } else if(delta == -1) {
    // global res is incorrect, while local res is correct
    if(whichPredictorToTake != SN) {
      chooserTable[globalHistory & globalMask]--;
    }
  }

  update_prediction_table(branchHistoryTable, globalHistory & globalMask, res);
  update_prediction_table(localPredcitionTable, localHistoryTable[pc & pcMask] & localMask, res);

  update_history(&globalHistory, res);
  update_history(&localHistoryTable[pc & pcMask], res);
}

int8_t
predict_gshare(uint32_t pc) {
  int index = (globalHistory ^ pc) & globalMask;
  return two_bit_predictor(branchHistoryTable[index]);
}

int8_t
predict_tournament(uint32_t pc) {
  int whichPredictorToTake = chooserTable[globalHistory & globalMask];
  if(whichPredictorToTake == WT || whichPredictorToTake == ST) {
    return two_bit_predictor(branchHistoryTable[globalHistory & globalMask]);
  } else {
    // two layers
    return two_bit_predictor(localPredcitionTable[localHistoryTable[pc & pcMask] & localMask]);
  }
}

// Initialize the local predictor
//
void
init_local_predictor() {
  localPredcitionTable = init_prediction_table(lhistoryBits, WN);
  localHistory = 0;
  localHistoryTable = init_history_table(pcIndexBits, localHistory);
  localMask = construct_mask(lhistoryBits);
}

// Initialize the tournament predictor
//
void
init_tournament_predictor() {
  init_gshare_predictor();
  init_local_predictor();
  // Take means take gloabl predictor
  chooserTable = init_history_table(ghistoryBits, WT);
  pcMask = construct_mask(pcIndexBits);
}

int*
init_prediction_table(int bits, uint8_t counterType) {
  int size = 1 << bits;
  int* table = (int*) malloc(sizeof(int)*size);
  for(int i=0; i<size; i++) {
    table[i] = counterType;
  }
  return table;
}

uint32_t *
init_history_table(int bits, uint32_t state) {
  uint32_t length = 1 << bits;
  uint32_t* table = (uint32_t *)malloc(sizeof(uint32_t) * length);
  for(uint32_t i = 0; i < length; ++i){
    table[i] = state;
  }

  return table;
}

void update_prediction_table(int* table, uint32_t index, uint8_t res) {
  if(res == TAKEN) {
    if(table[index] == ST) {
      return;
    }
    table[index]++;
  } else {
    if(table[index] == SN) {
      return;
    }
    table[index]--;
  }
}

uint32_t
update_history(uint32_t* history, uint8_t res) {
  *history <<= 1;
  *history += res;
}

// 2-bit predictor
//
uint8_t
two_bit_predictor(int counter) {
  if(counter == SN || counter == WN) {
    return NOTTAKEN;
  }
  return TAKEN;
}

void perceptron_shift(int16_t* satuate, uint8_t same){
  if(same){
    if(*satuate != ((1 << (PERCEPTRON_SATUATELEN - 1)) - 1)){
      (*satuate)++;
    }
  }else{
    if(*satuate != -(1 << (PERCEPTRON_SATUATELEN - 1 ) )){
      (*satuate)--;
    }
  }
}

void init_perceptron(){
  perceptronTrainTheta = (int32_t)(2.14 * PERCEPTRON_HEIGHT + 20);
  memset(perceptron, 0, sizeof(int16_t) * PCSIZE * (PERCEPTRON_HEIGHT + 1));
  memset(perceptronGHistory, 0, sizeof(uint16_t) * PERCEPTRON_HEIGHT);
}

int8_t predict_perceptron(uint32_t pc){
  uint32_t index = PERCEPTRON_MASK_PC(pc);
  int16_t out = perceptron[index][0];

  for(int i = 1 ; i <= PERCEPTRON_HEIGHT ; i++){
    out += perceptronGHistory[i-1] ? perceptron[index][i] : -perceptron[index][i];
  }

  perceptronRecentPrediction = (out >= 0) ? TAKEN : NOTTAKEN;
  perceptronNeedTrain = (out < perceptronTrainTheta && out > -perceptronTrainTheta) ? 1 : 0;

  return perceptronRecentPrediction;
}

void train_perceptron(uint32_t pc, uint8_t outcome){
  uint32_t index = PERCEPTRON_MASK_PC(pc);
  if((perceptronRecentPrediction != outcome) || perceptronNeedTrain){
    perceptron_shift(&(perceptron[index][0]), outcome);
    for(int i = 1 ; i <= PERCEPTRON_HEIGHT ; i++){
      uint8_t predict = perceptronGHistory[i-1];
      perceptron_shift(&(perceptron[index][i]), (outcome == predict));
    }
  }
  for(int i = PERCEPTRON_HEIGHT - 1; i > 0 ; i--){
    perceptronGHistory[i] = perceptronGHistory[i-1];
  }
  perceptronGHistory[0] = outcome;
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
{
  //
  //TODO: Implement prediction scheme
  //

  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      return predict_gshare(pc);
    case TOURNAMENT:
      return predict_tournament(pc);
    case CUSTOM:
      return predict_perceptron(pc);
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
void
train_predictor(uint32_t pc, uint8_t outcome)
{
  //
  //TODO: Implement Predictor training
  //
  switch (bpType) {
    case STATIC:
      break;
    case GSHARE:
      train_gshare(pc, outcome);
      break;
    case TOURNAMENT:
      train_tournament(pc, outcome);
      break;
    case CUSTOM:
      train_perceptron(pc, outcome);
    default:
      break;
  }
}
