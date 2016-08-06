/*
 * MIDIbox Quad Genesis: Synth engine
 *
 * ==========================================================================
 *
 *  Copyright (C) 2016 Sauraen (sauraen@gmail.com)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#include <mios32.h>
#include "syeng.h"

#include <genesis.h>
#include <vgm.h>
#include "app.h" //XXX
#include "demoprograms.h"

#define UNUSED_RECENCY 0x01000000ul //about 362 seconds ago

static const u8 use_scores[4] = {0, 1, 8, 100};

#define VOICECLEARVGMCMDS 45
static const u8 voiceclearvgm[VOICECLEARVGMCMDS*4] = {
    //Key off
    0x52, 0x28, 0x00, 0x00,
    //Set release rate to full so EG states return to 0 while we're writing the other stuff
    0x52, 0x80, 0xFF, 0x00,
    0x52, 0x84, 0xFF, 0x00,
    0x52, 0x88, 0xFF, 0x00,
    0x52, 0x8C, 0xFF, 0x00,
    //Turn on SSG mode so EG returns to 0 faster
    0x52, 0x90, 0x08, 0x00,
    0x52, 0x94, 0x08, 0x00,
    0x52, 0x98, 0x08, 0x00,
    0x52, 0x9C, 0x08, 0x00,
    //Set key scale rate to maximum so EG returns to 0 faster
    0x52, 0x50, 0xC0, 0x00,
    0x52, 0x54, 0xC0, 0x00,
    0x52, 0x58, 0xC0, 0x00,
    0x52, 0x5C, 0xC0, 0x00,
    //Set all channel registers to 0, except output
    0x52, 0xA4, 0x00, 0x00,
    0x52, 0xB0, 0x00, 0x00,
    0x52, 0xB4, 0xC0, 0x00,
    //Set all operator registers to 0
    0x52, 0x30, 0x00, 0x00,
    0x52, 0x34, 0x00, 0x00,
    0x52, 0x38, 0x00, 0x00,
    0x52, 0x3C, 0x00, 0x00,
    0x52, 0x40, 0x00, 0x00,
    0x52, 0x44, 0x00, 0x00,
    0x52, 0x48, 0x00, 0x00,
    0x52, 0x4C, 0x00, 0x00,
    0x52, 0x60, 0x00, 0x00,
    0x52, 0x64, 0x00, 0x00,
    0x52, 0x68, 0x00, 0x00,
    0x52, 0x6C, 0x00, 0x00,
    0x52, 0x70, 0x00, 0x00,
    0x52, 0x74, 0x00, 0x00,
    0x52, 0x78, 0x00, 0x00,
    0x52, 0x7C, 0x00, 0x00,
    //Wait a short time (~6 ms)
    0x61, 0x00, 0x00, 0x01,
    //Turn off KSR, SSG-EG, and reset release rates to zero
    0x52, 0x50, 0x00, 0x00,
    0x52, 0x54, 0x00, 0x00,
    0x52, 0x58, 0x00, 0x00,
    0x52, 0x5C, 0x00, 0x00,
    0x52, 0x90, 0x00, 0x00,
    0x52, 0x94, 0x00, 0x00,
    0x52, 0x98, 0x00, 0x00,
    0x52, 0x9C, 0x00, 0x00,
    0x52, 0x80, 0x00, 0x00,
    0x52, 0x84, 0x00, 0x00,
    0x52, 0x88, 0x00, 0x00,
    0x52, 0x8C, 0x00, 0x00
};


typedef struct voiceclearlink_s {
    VgmHead* head;
    struct voiceclearlink_s* link;
} voiceclearlink;

syngenesis_t syngenesis[GENESIS_COUNT];
synproginstance_t proginstances[MBQG_NUM_PROGINSTANCES];
synchannel_t channels[16*MBQG_NUM_PORTS];
u8 voiceclearfull;
static VgmSource* voiceclearsource;
static voiceclearlink* voiceclearlist;

//TODO divide chips that use globals by the types of globals used, so multiple
//voices using the same effect e.g. "ugly" can play together

static void VoiceReset(u8 g, u8 v){
    if(voiceclearfull){
        if(v >= 1 && v <= 6){
            DBG("Setting up g %d v %d to be cleared via VGM", g, v);
            syngenesis[g].channels[v].beingcleared = 1;
            voiceclearlink* link = vgmh2_malloc(sizeof(voiceclearlink));
            VgmHead* head = VGM_Head_Create(voiceclearsource, 0x1000, 0x1000);
            link->head = head;
            head->channel[1].map_chip = g;
            head->channel[1].map_voice = v-1;
            //Insert at head of queue
            //MIOS32_IRQ_Disable();
            link->link = voiceclearlist;
            voiceclearlist = link;
            //MIOS32_IRQ_Enable();
            //Start playing
            VGM_Head_Restart(head, VGM_Player_GetVGMTime());
            head->playing = 1;
        }else{
            VGM_ResetChipVoiceAsync(g, v);
        }
    }else{
        VGM_PartialResetChipVoiceAsync(g, v);
    }
}

static void ReleaseAllPI(synproginstance_t* pi){
    u8 i, g, v;
    VgmHead_Channel pimap;
    syngenesis_t* sg;
    for(i=0; i<12; ++i){
        pimap = pi->mapping[i];
        if(pimap.nodata) continue;
        g = pimap.map_chip;
        sg = &syngenesis[g];
        if(i == 0){
            //We were using OPN2 globals, so there can't be anything else
            //allocated on this OPN2 besides this PI
            sg->optionbits = 0;
            for(v=0; v<8; ++v){
                sg->channels[v].ALL = 0;
                VoiceReset(g, v);
            }
            //Skip to PSG section
            i = 7;
            continue;
        }
        if(i >= 1 && i <= 6){
            //FM voice
            v = pimap.map_voice+1;
            sg->channels[v].ALL = 0;
            VoiceReset(g, v);
            //See if this chip has any voice using LFO
            for(v=1; v<7; ++v) if(sg->channels[v].lfo) break;
            if(v == 7){ //No LFO used
                sg->lfovaries = 0;
                sg->lfofixed = 0;
            }
        }else if(i == 7){
            //DAC
            sg->channels[7].ALL = 0;
            VoiceReset(g, 7);
        }else if(i >= 8 && i <= 10){
            //SQ voice
            v = pimap.map_voice+8;
            sg->channels[v].ALL = 0;
            VoiceReset(g, v);
        }else{
            //Noise
            sg->channels[11].ALL = 0;
            sg->noisefreqsq3 = 0;
            VoiceReset(g, 11);
        }
    }
}

static void ClearPI(synproginstance_t* pi){
    //Stop actually playing
    if(pi->head != NULL){
        VGM_Head_Delete(pi->head);
        pi->head = NULL;
    }
    //Invalidate PI
    pi->valid = 0;
    pi->playing = 0;
    pi->playinginit = 0;
    pi->recency = VGM_Player_GetVGMTime() - UNUSED_RECENCY;
    //Release all resources
    ReleaseAllPI(pi);
}

void SyEng_ClearVoice(u8 g, u8 v){
    u8 use = syngenesis[g].channels[v].use;
    if(use == 0) return;
    if(use == 3){
        //Tracker voice
        return;
    }else{
        ClearPI(&proginstances[syngenesis[g].channels[v].pi_using]);
    }
    syngenesis[g].channels[v].use = 0;
}

static void StandbyPI(synproginstance_t* pi){
    u8 i, v;
    VgmHead_Channel pimap;
    syngenesis_t* sg;
    for(i=0; i<12; ++i){
        pimap = pi->mapping[i];
        if(pimap.nodata) continue;
        sg = &syngenesis[pimap.map_chip];
        if(i == 0){
            //We were using OPN2 globals
            for(v=0; v<8; ++v){
                sg->channels[v].use = 1;
            }
            //Skip to PSG section
            i = 7;
            continue;
        }
        if(i >= 1 && i <= 6){
            //FM voice
            v = pimap.map_voice;
            sg->channels[v+1].use = 1;
        }else if(i == 7){
            //DAC
            sg->channels[7].use = 1;
        }else if(i >= 8 && i <= 10){
            //SQ voice
            v = pimap.map_voice;
            sg->channels[v+8].use = 1;
        }else{
            //Noise
            sg->channels[11].use = 1;
        }
    }
}

static s8 FindOPN2ClearLFO(){
    //Find an OPN2 with as few voices as possible using the LFO
    u8 use, g, v, full;
    s8 bestg = -1;
    u16 score, bestscore;
    bestscore = 99;
    for(g=0; g<GENESIS_COUNT; ++g){
        score = 0;
        full = 1;
        for(v=1; v<6; ++v){
            use = syngenesis[g].channels[v].use;
            if(!use) full = 0;
            if(syngenesis[g].channels[v].lfo){
                //Only for voices using the LFO
                score += use_scores[use];
                full = 0;
            }
        }
        if(full){
            //All voices are taken, and none are using the LFO, so clearing
            //this one won't do us any good
            score = 0xFF;
        }
        if(score < bestscore){
            bestscore = score;
            bestg = g;
        }
    }
    if(bestg >= 0){
        //Kick out voices using the LFO
        for(v=1; v<6; ++v){
            if(syngenesis[bestg].channels[v].lfo){
                SyEng_ClearVoice(bestg, v);
            }
        }
    }
    return bestg;
}

static void AssignVoiceToGenesis(u8 piindex, synproginstance_t* pi, u8 g, u8 vsource, u8 vdest, u8 vlfo){
    //DBG("--Assigning PI %d voice %d to genesis %d voice %d, vlfo=%d", piindex, vsource, g, vdest, vlfo);
    syngenesis_usage_t* sgusage = &syngenesis[g].channels[vdest];
    SyEng_ClearVoice(g, vdest);
    //Assign voices
    u8 proper, map_voice;
    if(vdest >= 1 && vdest <= 6){
        proper = 1;
        map_voice = vdest - 1;
    }else if(vdest >= 8 && vdest <= 10){
        proper = 0;
        map_voice = vdest - 8;
    }else{
        proper = 0;
        map_voice = 0;
    }
    sgusage->use = 2;
    sgusage->pi_using = piindex;
    sgusage->lfo = vlfo && proper;
    //Map PI
    pi->mapping[vsource] = (VgmHead_Channel){.nodata = 0, .mute = 0, .map_chip = g, .map_voice = map_voice, .option = vlfo};
}

static s32 AllocatePI(u8 piindex, usage_bits_t pusage){
    synproginstance_t* pi = &proginstances[piindex];
    syngenesis_t* sg;
    u8 i, g, v, use, lfog, lfovaries;
    s8 bestg, bestv;
    u16 score, bestscore;
    u32 recency, maxrecency, now = VGM_Player_GetVGMTime();
    //Initialize channels to have no data
    for(i=0; i<12; ++i){
        pi->mapping[i] = (VgmHead_Channel){.nodata = 1, .mute = 0, .map_chip = 0, .map_voice = 0, .option = 0};
    }
    ////////////////////////////////////////////////////////////////////////////
    //////////////////////////// OPN2 ALLOCATION ///////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    if(pusage.opn2_globals){
        //We need an entire OPN2; find the best one to replace
        bestscore = 99;
        maxrecency = 0;
        bestg = -1;
        for(g=0; g<GENESIS_COUNT; ++g){
            score = 0;
            recency = 0;
            for(v=0; v<8; ++v){
                use = syngenesis[g].channels[v].use;
                score += use_scores[use];
                recency += (use >= 1) ? (now - proginstances[syngenesis[g].channels[v].pi_using].recency) : UNUSED_RECENCY;
            }
            if(score < bestscore || (score == bestscore && recency > maxrecency)){
                bestscore = score;
                bestg = g;
                maxrecency = recency;
            }
        }
        if(bestg < 0){
            ReleaseAllPI(pi);
            return -1;
        }
        //Replace this one
        sg = &syngenesis[bestg];
        for(v=0; v<8; ++v){
            AssignVoiceToGenesis(piindex, pi, bestg, v, v, 1);
        }
        //Set up additional bits in chip allocation record
        sg->lfovaries = 1;
        sg->lfofixed = 0;
    }else{
        if((pusage.all & 0x00000FC0) && !pusage.lfofixed){ //Any LFO used and not fixed
            lfovaries = 1;
            bestg = FindOPN2ClearLFO();
            if(bestg < 0){
                ReleaseAllPI(pi);
                return -2;
            }
            lfog = bestg;
            syngenesis[lfog].lfovaries = 1;
            syngenesis[lfog].lfofixed = 0;
        }else{
            lfovaries = 0;
            lfog = 0;
        }
        //Assign Ch6 if DAC used
        if(pusage.dac){
            if(pusage.fm6_lfo){
                if(lfovaries){
                    //LFO non-fixed; has to get assigned to lfog:6
                    AssignVoiceToGenesis(piindex, pi, lfog, 6, 6, 1);
                    AssignVoiceToGenesis(piindex, pi, lfog, 7, 7, 0);
                }else{
                    //LFO fixed; can we find an OPN2 with DAC open with the same LFO fixed?
                    for(g=0; g<GENESIS_COUNT; ++g){
                        sg = &syngenesis[lfog];
                        if(sg->lfofixed && sg->lfofixedspeed == pusage.lfofixedspeed && sg->channels[6].use == 0){
                            break;
                        }
                    }
                    if(g == GENESIS_COUNT){
                        //Clear an OPN2 of LFO
                        bestg = FindOPN2ClearLFO();
                        if(bestg < 0){
                            ReleaseAllPI(pi);
                            return -3;
                        }
                        g = bestg;
                        //Set it up to be fixed to us
                        syngenesis[g].lfovaries = 0;
                        syngenesis[g].lfofixed = 1;
                        syngenesis[g].lfofixedspeed = pusage.lfofixedspeed;
                        //Assign DAC to it, possibly overriding what was there
                    }
                    AssignVoiceToGenesis(piindex, pi, g, 6, 6, 1);
                    AssignVoiceToGenesis(piindex, pi, g, 7, 7, 1);
                }
            }else{
                //DAC without LFO
                //Find the best to replace
                bestscore = 99;
                maxrecency = 0;
                bestg = -1;
                for(g=0; g<GENESIS_COUNT; ++g){
                    score = 0;
                    recency = 0;
                    for(v=6; v<8; ++v){
                        use = syngenesis[g].channels[v].use;
                        score += use_scores[use];
                        recency += (use >= 1) ? (now - proginstances[syngenesis[g].channels[v].pi_using].recency) : UNUSED_RECENCY;
                    }
                    if(score < bestscore || (score == bestscore && recency > maxrecency)){
                        bestscore = score;
                        bestg = g;
                        maxrecency = recency;
                    }
                }
                if(bestg < 0){
                    ReleaseAllPI(pi);
                    return -4;
                }
                //Use bestg
                AssignVoiceToGenesis(piindex, pi, bestg, 6, 6, 0);
                AssignVoiceToGenesis(piindex, pi, bestg, 7, 7, 0);
            }
            pusage.fm6 = 0;
        }
        //Assign FM3 if FM3 Special mode
        if(pusage.fm3_special){
            if(pusage.fm3_lfo){
                if(lfovaries){
                    //LFO non-fixed; has to get assigned to lfog:3
                    AssignVoiceToGenesis(piindex, pi, lfog, 3, 3, 1);
                }else{
                    //LFO fixed; can we find an OPN2 with FM3 open with the same LFO fixed?
                    for(g=0; g<GENESIS_COUNT; ++g){
                        sg = &syngenesis[lfog];
                        if(sg->lfofixed && sg->lfofixedspeed == pusage.lfofixedspeed && sg->channels[3].use == 0){
                            break;
                        }
                    }
                    if(g == GENESIS_COUNT){
                        //Clear an OPN2 of LFO
                        bestg = FindOPN2ClearLFO();
                        if(bestg < 0){
                            ReleaseAllPI(pi);
                            return -5;
                        }
                        g = bestg;
                        //Set it up to be fixed to us
                        syngenesis[g].lfovaries = 0;
                        syngenesis[g].lfofixed = 1;
                        syngenesis[g].lfofixedspeed = pusage.lfofixedspeed;
                        //Assign FM3 to it, possibly overriding what was there
                    }
                    AssignVoiceToGenesis(piindex, pi, g, 3, 3, 1);
                }
            }else{
                //FM3 without LFO
                //Find the best to replace
                bestscore = 99;
                maxrecency = 0;
                bestg = -1;
                for(g=0; g<GENESIS_COUNT; ++g){
                    use = syngenesis[g].channels[3].use;
                    score = use_scores[use];
                    recency = (use >= 1) ? (now - proginstances[syngenesis[g].channels[3].pi_using].recency) : UNUSED_RECENCY;
                    if(score < bestscore || (score == bestscore && recency > maxrecency)){
                        bestscore = score;
                        bestg = g;
                        maxrecency = recency;
                    }
                }
                if(bestg < 0){
                    ReleaseAllPI(pi);
                    return -6;
                }
                //Use bestg
                AssignVoiceToGenesis(piindex, pi, bestg, 3, 3, 0);
            }
            pusage.fm3 = 0;
        }
        //Assign normal voices
        for(i=1; i<=6; ++i){
            if(pusage.all & (1 << (i-1))){ //Voice in use
                if(pusage.all & (1 << (i+5))){ //LFO in use
                    if(lfovaries){
                        //Have to use lfog, find best voice
                        bestscore = 99;
                        maxrecency = 0;
                        bestv = -1;
                        for(v=1; v<=6; ++v){
                            use = syngenesis[lfog].channels[v].use;
                            score = use_scores[use] << 1;
                            recency = (use >= 1) ? (now - proginstances[syngenesis[lfog].channels[v].pi_using].recency) : UNUSED_RECENCY;
                            if(v == 3 || v == 6) ++score;
                            if(score < bestscore || (score == bestscore && recency > maxrecency)){
                                bestscore = score;
                                bestv = v;
                                maxrecency = recency;
                            }
                        }
                        if(bestv < 0){
                            ReleaseAllPI(pi);
                            return -7;
                        }
                        //Use this voice
                        AssignVoiceToGenesis(piindex, pi, lfog, i, bestv, 1);
                    }else{
                        //LFO fixed: First is there a chip with LFO Fixed correct and a free voice?
                        bestscore = 99;
                        for(g=0; g<GENESIS_COUNT; ++g){
                            sg = &syngenesis[g];
                            if(!sg->lfofixed || sg->lfofixedspeed != pusage.lfofixedspeed) continue;
                            //If we have a chip with the right LFO Fixed, check the voices
                            score = 0xFF;
                            for(v=1; v<=6; ++v){
                                use = syngenesis[g].channels[v].use;
                                if(use <= 1){
                                    score = 0;
                                    break;
                                }
                            }
                            if(score == 0) break;
                        }
                        if(g == GENESIS_COUNT){
                            //Find the OPN2 with the least LFO use
                            bestg = FindOPN2ClearLFO();
                            if(bestg < 0){
                                ReleaseAllPI(pi);
                                return -8;
                            }
                            g = bestg;
                        }
                        //Find best voice
                        bestscore = 99;
                        maxrecency = 0;
                        bestv = -1;
                        for(v=1; v<=6; ++v){
                            use = syngenesis[g].channels[v].use;
                            score = use_scores[use] << 1;
                            recency = (use >= 1) ? (now - proginstances[syngenesis[g].channels[v].pi_using].recency) : UNUSED_RECENCY;
                            if(v == 3 || v == 6) ++score;
                            if(score < bestscore || (score == bestscore && recency > maxrecency)){
                                bestscore = score;
                                bestv = v;
                                maxrecency = recency;
                            }
                        }
                        if(bestv < 0){
                            ReleaseAllPI(pi);
                            return -9;
                        }
                        //Use this voice
                        AssignVoiceToGenesis(piindex, pi, g, i, bestv, 1);
                    }
                }else{
                    //No LFO, find best voice anywhere
                    bestscore = 99;
                    maxrecency = 0;
                    bestg = -1;
                    bestv = -1;
                    for(g=0; g<GENESIS_COUNT; ++g){
                        for(v=1; v<=6; ++v){
                            use = syngenesis[g].channels[v].use;
                            score = use_scores[use] << 1;
                            recency = (use >= 1) ? (now - proginstances[syngenesis[g].channels[v].pi_using].recency) : UNUSED_RECENCY;
                            if(v == 3 || v == 6) ++score;
                            if(score < bestscore || (score == bestscore && recency > maxrecency)){
                                bestscore = score;
                                bestv = v;
                                bestg = g;
                                maxrecency = recency;
                            }
                        }
                    }
                    if(bestg < 0){ // || bestv < 0
                        ReleaseAllPI(pi);
                        return -10;
                    }
                    //Use this voice
                    AssignVoiceToGenesis(piindex, pi, bestg, i, bestv, 0);
                }
            }
        }
    }
    if(pusage.noisefreqsq3){
        //Need SQ3 and NS together
        bestscore = 99;
        maxrecency = 0;
        bestg = -1;
        for(g=0; g<GENESIS_COUNT; ++g){
            use = syngenesis[g].channels[10].use;
            score = use_scores[use];
            recency = (use >= 1) ? (now - proginstances[syngenesis[g].channels[10].pi_using].recency) : UNUSED_RECENCY;
            use = syngenesis[g].channels[11].use;
            score += use_scores[use];
            recency += (use >= 1) ? (now - proginstances[syngenesis[g].channels[11].pi_using].recency) : UNUSED_RECENCY;
            if(score < bestscore || (score == bestscore && recency > maxrecency)){
                bestscore = score;
                bestg = g;
                maxrecency = recency;
            }
        }
        if(bestg < 0){
            ReleaseAllPI(pi);
            return -11;
        }
        //Use this chip
        AssignVoiceToGenesis(piindex, pi, bestg, 10, 10, 0);
        AssignVoiceToGenesis(piindex, pi, bestg, 11, 11, 0);
        //Mark these two as taken care of
        pusage.sq3 = 0;
        pusage.noise = 0;
    }
    if(pusage.noise){
        //Need NS
        bestscore = 99;
        maxrecency = 0;
        bestg = -1;
        for(g=0; g<GENESIS_COUNT; ++g){
            use = syngenesis[g].channels[11].use;
            score = use_scores[use];
            recency = (use >= 1) ? (now - proginstances[syngenesis[g].channels[11].pi_using].recency) : UNUSED_RECENCY;
            if(score < bestscore || (score == bestscore && recency > maxrecency)){
                bestscore = score;
                bestg = g;
                maxrecency = recency;
            }
        }
        if(bestg < 0){
            ReleaseAllPI(pi);
            return -12;
        }
        //Use this chip
        AssignVoiceToGenesis(piindex, pi, bestg, 11, 11, 0);
    }
    //Squares
    for(i=8; i<=10; ++i){
        if(pusage.all & (0x01000000 << (i-8))){ //Voice in use
            //Find best voice anywhere
            bestscore = 99;
            maxrecency = 0;
            bestg = -1;
            bestv = -1;
            for(g=0; g<GENESIS_COUNT; ++g){
                for(v=8; v<=10; ++v){
                    use = syngenesis[g].channels[v].use;
                    score = use_scores[use] << 1;
                    recency = (use >= 1) ? (now - proginstances[syngenesis[g].channels[v].pi_using].recency) : UNUSED_RECENCY;
                    if(v == 10) ++score;
                    if(score < bestscore || (score == bestscore && recency > maxrecency)){
                        bestscore = score;
                        bestv = v;
                        bestg = g;
                        maxrecency = recency;
                    }
                }
            }
            if(bestg < 0){
                ReleaseAllPI(pi);
                return -13;
            }
            //Use this voice
            AssignVoiceToGenesis(piindex, pi, bestg, i, bestv, 0);
        }
    }
    return 1;
}


static void CopyPIMappingToHead(synproginstance_t* pi, VgmHead* head){
    u8 i, m;
    for(i=0; i<12; ++i){
        m = head->channel[i].mute;
        head->channel[i].ALL = pi->mapping[i].ALL;
        head->channel[i].mute = m;
    }
}

static u8 IsAnyVoiceBeingCleared(synproginstance_t* pi){
    u8 c;
    if(pi->isstatic) return 0;
    for(c=1; c<7; ++c){
        VgmHead_Channel ch = pi->mapping[c];
        if(ch.nodata) continue;
        if(syngenesis[ch.map_chip].channels[ch.map_voice+1].beingcleared) return 1;
    }
    return 0;
}

static u8 FindBestPIToReplace(u8 chn, u8 note){
    //Look for an existing proginstance that's the best fit to replace
    //In order from best to worst:
    //0: Same channel, same note (playing or not playing doesn't matter)
    //1: Same channel, not playing
    //2: Invalid
    //3: Other channel, not playing
    //4: Same channel, playing
    //5: Other channel, playing
    u8 bestrating = 0xFF;
    u8 bestrated = 0xFF;
    u32 recency, maxrecency = 0, now = VGM_Player_GetVGMTime();
    u8 rating, i;
    synproginstance_t* pi;
    for(i=0; i<MBQG_NUM_PROGINSTANCES; ++i){
        pi = &proginstances[i];
        if(!pi->valid){
            rating = 2;
        }else{
            if(pi->sourcechannel == chn){
                if(pi->note == note){
                    rating = 0;
                }else if(pi->playing){
                    rating = 4;
                }else{
                    rating = 1;
                }
            }else{
                if(pi->playing){
                    rating = 5;
                }else{
                    rating = 3;
                }
            }
        }
        recency = now - pi->recency;
        if(rating < bestrating || (rating == bestrating && recency > maxrecency)){
            bestrating = rating;
            bestrated = i;
            maxrecency = recency;
        }
    }
    return bestrated;
}

void SyEng_Init(){
    u8 i, j;
    //Initialize syngenesis
    for(i=0; i<GENESIS_COUNT; ++i){
        syngenesis[i].optionbits = 0;
        for(j=0; j<12; ++j){
            syngenesis[i].channels[j].ALL = 0;
        }
    }
    //Initialize proginstances
    for(i=0; i<MBQG_NUM_PROGINSTANCES; ++i){
        proginstances[i].valid = 0;
        proginstances[i].head = NULL;
    }
    //Initialize channels
    for(i=0; i<16*MBQG_NUM_PORTS; ++i){
        channels[i].trackermode = 0;
        channels[i].program = NULL;
    }
    //Initialize OPN2 voice clearing
    voiceclearsource = VGM_SourceRAM_Create();
    ((VgmSourceRAM*)voiceclearsource->data)->numcmds = VOICECLEARVGMCMDS;
    ((VgmSourceRAM*)voiceclearsource->data)->cmds = (VgmChipWriteCmd*)voiceclearvgm;
    voiceclearlist = NULL;
    //Other
    voiceclearfull = 1;
    DemoPrograms_Init();
}


void SyEng_Tick(){
    u8 i;
    //Clear voiceclear VGMs
    //MIOS32_IRQ_Disable();
    voiceclearlink* link = voiceclearlist;
    voiceclearlink** backlink = &voiceclearlist;
    voiceclearlink* oldlink;
    while(link != NULL){
        if(link->head->isdone){
            //Mark the voice as clear
            VgmHead_Channel ch = link->head->channel[1];
            DBG("Clearing VGM for g %d v %d is done", ch.map_chip, ch.map_voice+1);
            syngenesis[ch.map_chip].channels[ch.map_voice+1].beingcleared = 0;
            //Delete the head
            VGM_Head_Delete(link->head);
            //Delete the link
            *backlink = link->link;
            oldlink = link;
            link = link->link;
            vgmh2_free(oldlink);
        }else{
            //Traverse the linked list
            backlink = &link->link;
            link = link->link;
        }
    }
    //MIOS32_IRQ_Enable();
    //Clear program heads, change init to noteon
    synproginstance_t* pi;
    synprogram_t* prog;
    for(i=0; i<MBQG_NUM_PROGINSTANCES; ++i){
        pi = &proginstances[i];
        if(!pi->valid) continue;
        if(pi->isstatic) continue;
        if(pi->waitingforclear){
            //Is it cleared yet?
            if(IsAnyVoiceBeingCleared(pi)) continue;
            //Done, start playing init VGM
            pi->waitingforclear = 0;
            prog = channels[pi->sourcechannel].program;
            if(prog == NULL){
                DBG("ERROR program disappeared while playing, could not start playing init after clearing!");
                continue;
            }
            if(prog->initsource != NULL){
                DBG("PI %d ch %d note %d done clearing, starting init VGM", i, pi->sourcechannel, pi->note);
                SyEng_PlayVGMOnPI(pi, prog->initsource, prog->rootnote);
            }else{
                pi->playinginit = 0;
                if(prog->noteonsource != NULL){
                    DBG("PI %d ch %d note %d done clearing, no init VGM, starting noteon VGM", i, pi->sourcechannel, pi->note);
                    SyEng_PlayVGMOnPI(pi, prog->noteonsource, prog->rootnote);
                }else{
                    DBG("PI %d ch %d note %d done clearing, but has no init or noteon VGM, doing nothing", i, pi->sourcechannel, pi->note);
                }
            }
            continue;
        }
        if(pi->head == NULL) continue;
        if(pi->head->isdone){
            VGM_Head_Delete(pi->head);
            pi->head = NULL;
            if(!pi->playinginit) continue;
            pi->playinginit = 0;
            prog = channels[pi->sourcechannel].program;
            if(prog == NULL){
                DBG("ERROR program disappeared while playing, could not switch from init to noteon!");
                continue;
            }
            //Switch from init to noteon VGM
            if(prog->noteonsource != NULL){
                DBG("PI %d ch %d note %d switching from init to noteon VGM", i, pi->sourcechannel, pi->note);
                SyEng_PlayVGMOnPI(pi, prog->noteonsource, prog->rootnote);
            }else{
                DBG("PI %d ch %d note %d done playing init, but has no noteon VGM, doing nothing", i, pi->sourcechannel, pi->note);
            }
        }
    }
}

u8 SyEng_GetStaticPI(usage_bits_t usage){
    u8 piindex = FindBestPIToReplace(0xFF, 0xFF);
    synproginstance_t* pi = &proginstances[piindex];
    //If we took away from another channel which was playing, release its resources
    if(pi->valid && (pi->playing == 1 || pi->isstatic)){
        DBG("--Clearing existing PI resources");
        ClearPI(pi);
    }
    if(pi->head != NULL){
        //Stop playing whatever it was playing
        VGM_Head_Delete(pi->head);
        pi->head = NULL;
    }
    //Find best allocation
    s32 ret = AllocatePI(piindex, usage);
    if(ret < 0){
        DBG("--Could not allocate resources for PI (voices full)! code = %d", ret);
        return 0xFF;
    }
    //Set up the PI
    pi->valid = 1;
    pi->isstatic = 1;
    pi->sourcechannel = 0xFF;
    pi->note = 60;
    return piindex;
}

void SyEng_ReleaseStaticPI(u8 piindex){
    synproginstance_t* pi = &proginstances[piindex];
    if(!pi->isstatic) return;
    if(pi->head != NULL){
        //Stop playing whatever it was playing
        VGM_Head_Delete(pi->head);
        pi->head = NULL;
    }
    pi->isstatic = 0;
    ClearPI(pi);
}

void SyEng_PlayVGMOnPI(synproginstance_t* pi, VgmSource* source, u8 rootnote){
    pi->head = VGM_Head_Create(source, VGM_getFreqMultiplier((s8)pi->note - (s8)rootnote), 0x1000);
    CopyPIMappingToHead(pi, pi->head);
    u32 vgmtime = VGM_Player_GetVGMTime();
    VGM_Head_Restart(pi->head, vgmtime);
    pi->head->playing = 1;
    pi->recency = vgmtime;
}

void SyEng_SilencePI(synproginstance_t* pi){
    u8 i;
    VgmHead_Channel* c;
    for(i=0; i<12; ++i){
        c = &pi->mapping[i];
        if(c->nodata) continue;
        //Key off
        if(i >= 1 && i <= 6){
            VGM_Tracker_Enqueue((VgmChipWriteCmd){.cmd = (c->map_chip << 4)|2, .addr = 0x28, .data = (c->map_voice >= 3 ? c->map_voice+1 : c->map_voice), .data2 = 0}, 0);
        }else if(i >= 8 && i <= 10){
            VGM_Tracker_Enqueue((VgmChipWriteCmd){.cmd = (c->map_chip << 4), .addr = 0, .data = 0x9F|(c->map_voice << 5), .data2 = 0}, 0);
        }else if(i == 11){
            VGM_Tracker_Enqueue((VgmChipWriteCmd){.cmd = (c->map_chip << 4), .addr = 0, .data = 0xFF, .data2 = 0}, 0);
        }
    }
}

static void StartProgramNote(synprogram_t* prog, u8 chn, u8 note){
    if(prog == NULL){
        DBG("Note on %d ch %d, no program on this channel", note, chn);
        return;
    }
    u8 piindex = FindBestPIToReplace(chn, note);
    synproginstance_t* pi = &proginstances[piindex];
    pi->note = note;
    //Do we already have the right mapping allocated?
    if(pi->valid && !pi->isstatic && pi->sourcechannel == chn){
        //Skip the init VGM, start the note on VGM
        if(pi->head != NULL){
            //Stop playing whatever it was playing
            VGM_Head_Delete(pi->head);
            pi->head = NULL;
        }
        if(prog->noteonsource != NULL){
            SyEng_PlayVGMOnPI(pi, prog->noteonsource, prog->rootnote);
        }else{
            DBG("PI ch %d note %d doesn't need init, but has no noteon VGM, doing nothing", pi->sourcechannel, pi->note);
        }
        pi->playing = 1;
        return;
    }
    //If we took away from another channel which was playing, release its resources
    if(pi->valid && (pi->playing == 1 || pi->isstatic)){
        DBG("--Clearing existing PI resources");
        ClearPI(pi);
    }
    if(pi->head != NULL){
        //Stop playing whatever it was playing
        VGM_Head_Delete(pi->head);
        pi->head = NULL;
    }
    //Find best allocation
    s32 ret = AllocatePI(piindex, prog->usage);
    if(ret < 0){
        DBG("--Could not allocate resources for PI (voices full)! code = %d", ret);
        return;
    }
    //Set up the PI for the new program
    pi->valid = 1;
    pi->isstatic = 0;
    pi->playinginit = 1;
    pi->playing = 1;
    pi->sourcechannel = chn;
    //See if we're waiting for voices to be cleared
    if(IsAnyVoiceBeingCleared(pi)){
        pi->waitingforclear = 1;
        return; //Init will be played in SyEng_Tick
    }
    //Otherwise, start the init VGM
    if(prog->initsource != NULL){
        SyEng_PlayVGMOnPI(pi, prog->initsource, prog->rootnote);
    }else{
        pi->playinginit = 0;
        if(prog->noteonsource != NULL){
            DBG("PI ch %d note %d skipping missing init VGM, starting noteon", pi->sourcechannel, pi->note);
            SyEng_PlayVGMOnPI(pi, prog->noteonsource, prog->rootnote);
        }else{
            DBG("PI ch %d note %d doesn't have init or noteon VGMs, doing nothing", pi->sourcechannel, pi->note);
        }
    }
}
static void StopProgramNote(synprogram_t* prog, u8 chn, u8 note){
    synproginstance_t* pi;
    u8 i;
    for(i=0; i<MBQG_NUM_PROGINSTANCES; ++i){
        pi = &proginstances[i];
        if(!pi->valid) continue;
        if(pi->isstatic) continue;
        if(pi->sourcechannel != chn) continue;
        if(!pi->playing) continue;
        if(pi->note != note) continue;
        break;
    }
    if(i == MBQG_NUM_PROGINSTANCES){
        DBG("Note off %d ch %d, but no PI playing this note", note, chn);
        return; //no corresponding note on
    }
    //Found corresponding note on
    if(prog == NULL){
        DBG("--ERROR program disappeared while playing, could not switch to noteoff!");
        return;
    }
    if(prog->noteoffsource != NULL){
        //Stop playing note-on VGM only if there's a noteoff to play
        if(pi->head != NULL){
            VGM_Head_Delete(pi->head);
            pi->head = NULL;
        }
        //Start playing note-off VGM
        SyEng_PlayVGMOnPI(pi, prog->noteoffsource, prog->rootnote);
    }else{
        DBG("PI ch %d note %d doesn't have noteoff VGM, doing nothing", pi->sourcechannel, pi->note);
    }
    //Mark pi as not playing, release resources
    StandbyPI(pi);
    pi->playing = 0;
}

void SyEng_Note_On(mios32_midi_package_t pkg){
    StartProgramNote(channels[pkg.chn].program, pkg.chn, pkg.note);
}
void SyEng_Note_Off(mios32_midi_package_t pkg){
    StopProgramNote(channels[pkg.chn].program, pkg.chn, pkg.note);
}

