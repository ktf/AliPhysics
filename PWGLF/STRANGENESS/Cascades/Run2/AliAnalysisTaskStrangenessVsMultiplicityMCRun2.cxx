/**************************************************************************
 * Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
 *                                                                        *
 * Author: The ALICE Off-line Project.                                    *
 * Contributors are mentioned in the code where appropriate.              *
 *                                                                        *
 * Permission to use, copy, modify and distribute this software and its   *
 * documentation strictly for non-commercial purposes is hereby granted   *
 * without fee, provided that the above copyright notice appears in all   *
 * copies and that both the copyright notice and this permission notice   *
 * appear in the supporting documentation. The authors make no claims     *
 * about the suitability of this software for any purpose. It is          *
 * provided "as is" without express or implied warranty.                  *
 **************************************************************************/

// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// This task is meant to provide a lightweight alternative to the regular
// V0 and cascade analysis tasks that were used in Run 1, which produced full-size
// TTree objects for V0 and cascade candidates. Instead of that, the output
// for this task has been structured as follows:
//
//  Output 1: TList object containing some standard analysis histograms
//            for event counting.
//
//  Output 2: TList object containing all registered output objects for
//            the V0 analysis in AliV0Result format. The output objects will
//            each hold a TH3F with analysis-relevant information and the
//            configuration that was used to get to that.
//
//  Output 3: TList object containing all registered output objects for
//            the cascade analysis in AliCascadeResult format. The output objects will
//            each hold a TH3F with analysis-relevant information and the
//            configuration that was used to get to that.
//
//  Output 4: (optional) TTree object holding event characteristics. At the
//            moment only related to a single centrality estimator (default:
//            V0M). No downscaling option exists.
//
//  Output 5: (optional) TTree object containing V0 candidates to allow for
//            Run 1 legacy code to function and to allow for full information
//            to be saved. To keep output under control, a downscaling factor
//            can be applied (default: 0.001, configurable)
//
//  Output 6: (optional) TTree object containing cascade candidates to allow for
//            Run 1 legacy code to function and to allow for full information
//            to be saved. To keep output under control, a downscaling factor
//            can be applied (default: 0.001, configurable)
//
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class TTree;
class TParticle;
class TVector3;

//class AliMCEventHandler;
//class AliMCEvent;
//class AliStack;

class AliESDVertex;
class AliAODVertex;
class AliESDv0;
class AliAODv0;

#include <numeric>

#include <Riostream.h>
#include "TList.h"
#include "TH1.h"
#include "TH2.h"
#include "TH3.h"
#include "TFile.h"
#include "THnSparse.h"
#include "TVector3.h"
#include "TCanvas.h"
#include "TMath.h"
#include "TLegend.h"
#include "TRandom3.h"
#include "TProfile.h"
//#include "AliLog.h"

#include "AliESDEvent.h"
#include "AliAODEvent.h"
#include "AliV0vertexer.h"
#include "AliCascadeVertexer.h"
#include "AliLightV0vertexer.h"
#include "AliLightCascadeVertexer.h"
#include "AliESDpid.h"
#include "AliExternalTrackParam.h"
#include "AliESDtrack.h"
#include "AliESDtrackCuts.h"
#include "AliInputEventHandler.h"
#include "AliAnalysisManager.h"
#include "AliMCEventHandler.h"
#include "AliMCEvent.h"
#include "AliStack.h"
#include "AliCentrality.h"
#include "AliOADBContainer.h"
#include "AliOADBMultSelection.h"
#include "AliMultEstimator.h"
#include "AliMultVariable.h"
#include "AliMultInput.h"
#include "AliMultSelection.h"

#include "AliAODForwardMult.h"
#include "AliForwardUtil.h"
#include "AliCFContainer.h"
#include "AliMultiplicity.h"
#include "AliAODMCParticle.h"
#include "AliESDcascade.h"
#include "AliAODcascade.h"
#include "AliESDUtils.h"
#include "AliGenEventHeader.h"
#include "AliAnalysisTaskSE.h"
#include "AliAnalysisUtils.h"
#include "AliEventCuts.h"
#include "AliV0Result.h"
#include "AliCascadeResult.h"
#include "AliAnalysisTaskStrangenessVsMultiplicityMCRun2.h"

using std::cout;
using std::endl;

ClassImp(AliAnalysisTaskStrangenessVsMultiplicityMCRun2)

AliAnalysisTaskStrangenessVsMultiplicityMCRun2::AliAnalysisTaskStrangenessVsMultiplicityMCRun2()
: AliAnalysisTaskSE(), fListHist(0), fListV0(0), fListCascade(0), fTreeEvent(0), fTreeV0(0), fTreeCascade(0), fPIDResponse(0), fESDtrackCuts(0), fESDtrackCutsITSsa2010(0), fESDtrackCutsGlobal2015(0), fUtils(0), fRand(0),

//---> Flags controlling Event Tree output
fkSaveEventTree    ( kTRUE ), //no downscaling in this tree so far

//---> Flags controlling V0 TTree output
fkSaveV0Tree       ( kTRUE ),
fkDownScaleV0      ( kTRUE  ),
fDownScaleFactorV0 ( 0.001  ),
fkPreselectDedx ( kFALSE ),
fkPreselectPID  ( kTRUE  ),
fkUseOnTheFlyV0Cascading( kFALSE ),
fkDoImprovedCascadeVertexFinding(kFALSE),
fkIfImprovedPerformInitialLinearPropag( kFALSE ),
fkIfImprovedExtraPrecisionFactor ( 1.0 ),
fkDebugWrongPIDForTracking ( kFALSE ),
fkDebugBump( kFALSE ),
fkDebugOOBPileup(kFALSE),
fkDoExtraEvSels( kTRUE ),

//---> Flags controlling Cascade TTree output
fkSaveCascadeTree       ( kTRUE  ),
fkDownScaleCascade      ( kTRUE  ),
fDownScaleFactorCascade ( 0.001  ),
fMinPtToSave( 0.00   ) ,
fMaxPtToSave( 100.00 ) ,

//---> Flags controlling sandbox mode (cascade)
fkSandboxMode( kFALSE ),

//---> Variables for Sibling Tagging
fSibCutDcaV0ToPrimVertex       ( 0.8    ),
fSibCutDcaV0Daughters          ( 0.15   ),
fSibCutV0CosineOfPointingAngle ( 0.995  ),
fSibCutV0Radius                ( 14.    ),
fSibCutDcaPosToPrimVertex      ( 5.     ),
fSibCutDcaNegToPrimVertex      ( 5.     ),
fSibCutInvMassK0s              ( 0.0075 ),

//---> Flags controlling Vertexers
fkRunVertexers    ( kFALSE ),
fkUseLightVertexer ( kTRUE ),
fkDoV0Refit ( kTRUE ),
fkExtraCleanup    ( kTRUE ),

//---> Flag controlling trigger selection
fTrigType(AliVEvent::kMB),

//---> Variables for fTreeEvent
fCentrality(0),
fMVPileupFlag(kFALSE),
fOOBPileupFlag(kFALSE),
fNTOFClusters(-1),
fNTOFMatches(-1),
fNTracksITSsa2010(-1),
fNTracksGlobal2015(-1),
fNTracksGlobal2015TriggerPP(-1),
fAmplitudeV0A(-1.),
fAmplitudeV0C(-1.),
fNHitsFMDA(-1.),
fNHitsFMDC(-1.),

//---> Variables for fTreeV0
fTreeVariableChi2V0(0),
fTreeVariableDcaV0Daughters(0),
fTreeVariableDcaV0ToPrimVertex(0),
fTreeVariableDcaPosToPrimVertex(0),
fTreeVariableDcaNegToPrimVertex(0),
fTreeVariableV0CosineOfPointingAngle(0),
fTreeVariableV0Radius(0),
fTreeVariablePt(0),
fTreeVariablePtMC(0),
fTreeVariableRapK0Short(0),
fTreeVariableRapLambda(0),
fTreeVariableRapMC(0),
fTreeVariableInvMassK0s(0),
fTreeVariableInvMassLambda(0),
fTreeVariableInvMassAntiLambda(0),
fTreeVariableAlphaV0(0),
fTreeVariablePtArmV0(0),
fTreeVariableNegEta(0),
fTreeVariablePosEta(0),

fTreeVariableNSigmasPosProton(0),
fTreeVariableNSigmasPosPion(0),
fTreeVariableNSigmasNegProton(0),
fTreeVariableNSigmasNegPion(0),

fTreeVariableDistOverTotMom(0),
fTreeVariableLeastNbrCrossedRows(0),
fTreeVariableLeastRatioCrossedRowsOverFindable(0),
fTreeVariableMaxChi2PerCluster(0),
fTreeVariableMinTrackLength(0),

fTreeVariablePosPIDForTracking(-1),
fTreeVariableNegPIDForTracking(-1),
fTreeVariablePosdEdx(-1),
fTreeVariableNegdEdx(-1),
fTreeVariablePosInnerP(-1),
fTreeVariableNegInnerP(-1),
fTreeVariableNegTrackStatus(0),
fTreeVariablePosTrackStatus(0),
fTreeVariableNegDCAz(-1),
fTreeVariablePosDCAz(-1),

fTreeVariableNegTOFExpTDiff(99999),
fTreeVariablePosTOFExpTDiff(99999),
fTreeVariableNegTOFSignal(99999),
fTreeVariablePosTOFSignal(99999),
fTreeVariableAmplitudeV0A(-1.),
fTreeVariableAmplitudeV0C(-1.),
fTreeVariableNHitsFMDA(-1.),
fTreeVariableNHitsFMDC(-1.),

fTreeVariableCentrality(0),
fTreeVariableMVPileupFlag(kFALSE),
fTreeVariableOOBPileupFlag(kFALSE),
//MC Variables
fTreeVariablePtMother(0),
fTreeVariableRapMother(0),
fTreeVariablePID(0),
fTreeVariablePIDPositive(0),
fTreeVariablePIDNegative(0),
fTreeVariablePIDMother(0),
fTreeVariablePrimaryStatus(0),
fTreeVariablePrimaryStatusMother(0),

//---> Variables for fTreeCascade
fTreeCascVarCharge(0),
fTreeCascVarMassAsXi(0),
fTreeCascVarMassAsOmega(0),
fTreeCascVarPt(0),
fTreeCascVarPtMC(0),
fTreeCascVarRapXi(0),
fTreeCascVarRapOmega(0),
fTreeCascVarRapMC(0),
fTreeCascVarNegEta(0),
fTreeCascVarPosEta(0),
fTreeCascVarBachEta(0),
fTreeCascVarDCACascDaughters(0),
fTreeCascVarDCABachToPrimVtx(0),
fTreeCascVarDCAV0Daughters(0),
fTreeCascVarDCAV0ToPrimVtx(0),
fTreeCascVarDCAPosToPrimVtx(0),
fTreeCascVarDCANegToPrimVtx(0),
fTreeCascVarCascCosPointingAngle(0),
fTreeCascVarCascDCAtoPVxy(0),
fTreeCascVarCascDCAtoPVz(0),
fTreeCascVarCascRadius(0),
fTreeCascVarV0Mass(0),
fTreeCascVarV0CosPointingAngle(0),
fTreeCascVarV0CosPointingAngleSpecial(0),
fTreeCascVarV0Radius(0),
fTreeCascVarDCABachToBaryon(0),
fTreeCascVarWrongCosPA(0),
fTreeCascVarLeastNbrClusters(0),
fTreeCascVarDistOverTotMom(0),
fTreeCascVarMaxChi2PerCluster(0),
fTreeCascVarMinTrackLength(0),

fTreeCascVarNegNSigmaPion(0),
fTreeCascVarNegNSigmaProton(0),
fTreeCascVarPosNSigmaPion(0),
fTreeCascVarPosNSigmaProton(0),
fTreeCascVarBachNSigmaPion(0),
fTreeCascVarBachNSigmaKaon(0),

fTreeCascVarChiSquareV0(1e+3),
fTreeCascVarChiSquareCascade(1e+3),
fTreeCascVarPosPIDForTracking(-1),
fTreeCascVarNegPIDForTracking(-1),
fTreeCascVarBachPIDForTracking(-1),
fTreeCascVarNegInnerP(-1),
fTreeCascVarPosInnerP(-1),
fTreeCascVarBachInnerP(-1),
fTreeCascVarNegdEdx(-1),
fTreeCascVarPosdEdx(-1),
fTreeCascVarBachdEdx(-1),

fTreeCascVarNegTrackStatus(0), //!
fTreeCascVarPosTrackStatus(0), //!
fTreeCascVarBachTrackStatus(0), //!
fTreeCascVarNegDCAz(-1),
fTreeCascVarPosDCAz(-1),
fTreeCascVarBachDCAz(-1),

//Variables for debugging the invariant mass bump
//Full momentum information
fTreeCascVarNegPx(0),
fTreeCascVarNegPy(0),
fTreeCascVarNegPz(0),
fTreeCascVarPosPx(0),
fTreeCascVarPosPy(0),
fTreeCascVarPosPz(0),
fTreeCascVarBachPx(0),
fTreeCascVarBachPy(0),
fTreeCascVarBachPz(0),
fTreeCascVarNegPxMC(0),
fTreeCascVarNegPyMC(0),
fTreeCascVarNegPzMC(0),
fTreeCascVarPosPxMC(0),
fTreeCascVarPosPyMC(0),
fTreeCascVarPosPzMC(0),
fTreeCascVarBachPxMC(0),
fTreeCascVarBachPyMC(0),
fTreeCascVarBachPzMC(0),
fTreeCascVarV0DecayX(0),
fTreeCascVarV0DecayY(0),
fTreeCascVarV0DecayZ(0),
fTreeCascVarCascadeDecayX(0),
fTreeCascVarCascadeDecayY(0),
fTreeCascVarCascadeDecayZ(0),
fTreeCascVarV0DecayXMC(0),
fTreeCascVarV0DecayYMC(0),
fTreeCascVarV0DecayZMC(0),
fTreeCascVarCascadeDecayXMC(0),
fTreeCascVarCascadeDecayYMC(0),
fTreeCascVarCascadeDecayZMC(0),
fTreeCascVarBachelorDCAptX(0),
fTreeCascVarBachelorDCAptY(0),
fTreeCascVarBachelorDCAptZ(0),
fTreeCascVarV0DCAptX(0),
fTreeCascVarV0DCAptY(0),
fTreeCascVarV0DCAptZ(0),
fTreeCascVarDCADaughters_Test(0),
fTreeCascVarBachelorDCAptSigmaX2(0),
fTreeCascVarBachelorDCAptSigmaY2(0),
fTreeCascVarBachelorDCAptSigmaZ2(0),
fTreeCascVarV0DCAptUncertainty_V0Pos(0),
fTreeCascVarV0DCAptUncertainty_V0Ang(0),
fTreeCascVarV0DCAptPosSigmaX2(0),
fTreeCascVarV0DCAptPosSigmaY2(0),
fTreeCascVarV0DCAptPosSigmaZ2(0),
fTreeCascVarV0DCAptPosSigmaSnp2(0),
fTreeCascVarV0DCAptPosSigmaTgl2(0),
fTreeCascVarV0DCAptNegSigmaX2(0),
fTreeCascVarV0DCAptNegSigmaY2(0),
fTreeCascVarV0DCAptNegSigmaZ2(0),
fTreeCascVarV0DCAptNegSigmaSnp2(0),
fTreeCascVarV0DCAptNegSigmaTgl2(0),
fTreeCascVarBachDCAPVSigmaX2(0),
fTreeCascVarBachDCAPVSigmaY2(0),
fTreeCascVarBachDCAPVSigmaZ2(0),
fTreeCascVarPosDCAPVSigmaX2(0),
fTreeCascVarPosDCAPVSigmaY2(0),
fTreeCascVarPosDCAPVSigmaZ2(0),
fTreeCascVarNegDCAPVSigmaX2(0),
fTreeCascVarNegDCAPVSigmaY2(0),
fTreeCascVarNegDCAPVSigmaZ2(0),
fTreeCascVarPrimVertexX(0),
fTreeCascVarPrimVertexY(0),
fTreeCascVarPrimVertexZ(0),
fTreeCascVarMagField(0),
fTreeCascVarV0Lifetime(0),
fTreeCascVarV0ChiSquare(0),

//Track Labels (check for duplicates, etc)
fTreeCascVarNegIndex(0),
fTreeCascVarPosIndex(0),
fTreeCascVarBachIndex(0),
fTreeCascVarNegLabel(0),
fTreeCascVarPosLabel(0),
fTreeCascVarBachLabel(0),
fTreeCascVarNegLabelMother(0),
fTreeCascVarPosLabelMother(0),
fTreeCascVarBachLabelMother(0),
fTreeCascVarNegLabelGrandMother(0),
fTreeCascVarPosLabelGrandMother(0),
fTreeCascVarBachLabelGrandMother(0),

//Event Number (check same-event index mixups)
fTreeCascVarEventNumber(0),
fTreeCascVarNegTOFExpTDiff(99999),
fTreeCascVarPosTOFExpTDiff(99999),
fTreeCascVarBachTOFExpTDiff(99999),
fTreeCascVarNegTOFSignal(99999),
fTreeCascVarPosTOFSignal(99999),
fTreeCascVarBachTOFSignal(99999),
fTreeCascVarAmplitudeV0A(-1.),
fTreeCascVarAmplitudeV0C(-1.),
fTreeCascVarNHitsFMDA(-1.),
fTreeCascVarNHitsFMDC(-1.),


fTreeCascVarCentrality(0),
fTreeCascVarMVPileupFlag(kFALSE),
fTreeCascVarOOBPileupFlag(kFALSE),

fTreeCascVarPID(0),
fTreeCascVarPIDNegative(0),
fTreeCascVarPIDPositive(0),
fTreeCascVarPIDBachelor(0),
fTreeCascVarPIDNegativeMother(0),
fTreeCascVarPIDPositiveMother(0),
fTreeCascVarPIDBachelorMother(0),
fTreeCascVarPIDNegativeGrandMother(0),
fTreeCascVarPIDPositiveGrandMother(0),
fTreeCascVarPIDBachelorGrandMother(0),

fTreeCascVarBachCousinStatus(0),
fTreeCascVarV0BachSibIsValid(0),
fTreeCascVarBachV0Tagging(0),
fTreeCascVarV0NegSibIsValid(0),
fTreeCascVarNegV0Tagging(0),
fTreeCascVarV0PosSibIsValid(0),
fTreeCascVarPosV0Tagging(0),

fTreeCascVarBachSibPt(0),
fTreeCascVarBachSibDcaV0ToPrimVertex(0),
fTreeCascVarBachSibDcaV0Daughters(0),
fTreeCascVarBachSibV0CosineOfPointingAngle(0),
fTreeCascVarBachSibV0V0Radius(0),
fTreeCascVarBachSibV0DcaPosToPrimVertex(0),
fTreeCascVarBachSibV0DcaNegToPrimVertex(0),
fTreeCascVarBachSibV0InvMassK0s(0),
fTreeCascVarBachSibV0InvMassLambda(0),
fTreeCascVarBachSibV0InvMassAntiLambda(0),

fTreeCascVarNegSibPt(0),
fTreeCascVarNegSibDcaV0ToPrimVertex(0),
fTreeCascVarNegSibDcaV0Daughters(0),
fTreeCascVarNegSibV0CosineOfPointingAngle(0),
fTreeCascVarNegSibV0V0Radius(0),
fTreeCascVarNegSibV0DcaPosToPrimVertex(0),
fTreeCascVarNegSibV0DcaNegToPrimVertex(0),
fTreeCascVarNegSibV0InvMassK0s(0),
fTreeCascVarNegSibV0InvMassLambda(0),
fTreeCascVarNegSibV0InvMassAntiLambda(0),

fTreeCascVarPosSibPt(0),
fTreeCascVarPosSibDcaV0ToPrimVertex(0),
fTreeCascVarPosSibDcaV0Daughters(0),
fTreeCascVarPosSibV0CosineOfPointingAngle(0),
fTreeCascVarPosSibV0V0Radius(0),
fTreeCascVarPosSibV0DcaPosToPrimVertex(0),
fTreeCascVarPosSibV0DcaNegToPrimVertex(0),
fTreeCascVarPosSibV0InvMassK0s(0),
fTreeCascVarPosSibV0InvMassLambda(0),
fTreeCascVarPosSibV0InvMassAntiLambda(0),

fTreeCascVarIsPhysicalPrimary(0),

fTreeCascVarIsPhysicalPrimaryNegative(kFALSE),
fTreeCascVarIsPhysicalPrimaryPositive(kFALSE),
fTreeCascVarIsPhysicalPrimaryBachelor(kFALSE),
fTreeCascVarIsPhysicalPrimaryNegativeMother(kFALSE),
fTreeCascVarIsPhysicalPrimaryPositiveMother(kFALSE),
fTreeCascVarIsPhysicalPrimaryBachelorMother(kFALSE),
fTreeCascVarIsPhysicalPrimaryNegativeGrandMother(kFALSE),
fTreeCascVarIsPhysicalPrimaryPositiveGrandMother(kFALSE),
fTreeCascVarIsPhysicalPrimaryBachelorGrandMother(kFALSE),

fTreeCascVarPosITSClusters0(0),
fTreeCascVarPosITSClusters1(0),
fTreeCascVarPosITSClusters2(0),
fTreeCascVarPosITSClusters3(0),
fTreeCascVarPosITSClusters4(0),
fTreeCascVarPosITSClusters5(0),

fTreeCascVarNegITSClusters0(0),
fTreeCascVarNegITSClusters1(0),
fTreeCascVarNegITSClusters2(0),
fTreeCascVarNegITSClusters3(0),
fTreeCascVarNegITSClusters4(0),
fTreeCascVarNegITSClusters5(0),

fTreeCascVarBachITSClusters0(0),
fTreeCascVarBachITSClusters1(0),
fTreeCascVarBachITSClusters2(0),
fTreeCascVarBachITSClusters3(0),
fTreeCascVarBachITSClusters4(0),
fTreeCascVarBachITSClusters5(0),

fTreeCascVarPosITSSharedClusters0(0),
fTreeCascVarPosITSSharedClusters1(0),
fTreeCascVarPosITSSharedClusters2(0),
fTreeCascVarPosITSSharedClusters3(0),
fTreeCascVarPosITSSharedClusters4(0),
fTreeCascVarPosITSSharedClusters5(0),

fTreeCascVarNegITSSharedClusters0(0),
fTreeCascVarNegITSSharedClusters1(0),
fTreeCascVarNegITSSharedClusters2(0),
fTreeCascVarNegITSSharedClusters3(0),
fTreeCascVarNegITSSharedClusters4(0),
fTreeCascVarNegITSSharedClusters5(0),

fTreeCascVarBachITSSharedClusters0(0),
fTreeCascVarBachITSSharedClusters1(0),
fTreeCascVarBachITSSharedClusters2(0),
fTreeCascVarBachITSSharedClusters3(0),
fTreeCascVarBachITSSharedClusters4(0),
fTreeCascVarBachITSSharedClusters5(0),

//Uncertainty information on mass (from KF) for testing purposes
fTreeCascVarV0LambdaMassError(0),
fTreeCascVarV0AntiLambdaMassError(0),

fTreeCascVarBachIsKink(0),
fTreeCascVarPosIsKink(0),
fTreeCascVarNegIsKink(0),

fTreeCascVarSwappedPID(0),

//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//Save full info for full re-vertex offline replay ('sandbox mode')
//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
fTreeCascVarBachTrack(0),
fTreeCascVarPosTrack(0),
fTreeCascVarNegTrack(0),
fTreeCascVarMagneticField(0),

//Histos
fHistEventCounter(0),
fHistCentrality(0),
//V0s
fHistGeneratedPtVsYVsCentralityK0Short(0),
fHistGeneratedPtVsYVsCentralityLambda(0),
fHistGeneratedPtVsYVsCentralityAntiLambda(0),
//Cascades
fHistGeneratedPtVsYVsCentralityXiMinus(0),
fHistGeneratedPtVsYVsCentralityXiPlus(0),
fHistGeneratedPtVsYVsCentralityOmegaMinus(0),
fHistGeneratedPtVsYVsCentralityOmegaPlus(0)
//------------------------------------------------
// Tree Variables
{
    
}

AliAnalysisTaskStrangenessVsMultiplicityMCRun2::AliAnalysisTaskStrangenessVsMultiplicityMCRun2(Bool_t lSaveEventTree, Bool_t lSaveV0Tree, Bool_t lSaveCascadeTree, const char *name, TString lExtraOptions)
: AliAnalysisTaskSE(name), fListHist(0), fListV0(0), fListCascade(0), fTreeEvent(0), fTreeV0(0), fTreeCascade(0), fPIDResponse(0), fESDtrackCuts(0), fESDtrackCutsITSsa2010(0), fESDtrackCutsGlobal2015(0), fUtils(0), fRand(0),

//---> Flags controlling Event Tree output
fkSaveEventTree    ( kTRUE ), //no downscaling in this tree so far

//---> Flags controlling V0 TTree output
fkSaveV0Tree       ( kTRUE ),
fkDownScaleV0      ( kTRUE  ),
fDownScaleFactorV0 ( 0.001  ),
fkPreselectDedx ( kFALSE ),
fkPreselectPID  ( kTRUE  ),
fkUseOnTheFlyV0Cascading( kFALSE ),
fkDoImprovedCascadeVertexFinding(kFALSE),
fkIfImprovedPerformInitialLinearPropag( kFALSE ),
fkIfImprovedExtraPrecisionFactor ( 1.0 ),
fkDebugWrongPIDForTracking ( kFALSE ),
fkDebugBump( kFALSE ),
fkDebugOOBPileup(kFALSE),
fkDoExtraEvSels( kTRUE ),

//---> Flags controlling Cascade TTree output
fkSaveCascadeTree       ( kTRUE  ),
fkDownScaleCascade      ( kTRUE  ),
fDownScaleFactorCascade ( 0.001  ),
fMinPtToSave( 0.00   ) ,
fMaxPtToSave( 100.00 ) ,

//---> Flags controlling sandbox mode (cascade)
fkSandboxMode( kFALSE ),

//---> Variables for Sibling Tagging
fSibCutDcaV0ToPrimVertex       ( 0.8    ),
fSibCutDcaV0Daughters          ( 0.15   ),
fSibCutV0CosineOfPointingAngle ( 0.995  ),
fSibCutV0Radius                ( 14.    ),
fSibCutDcaPosToPrimVertex      ( 5.     ),
fSibCutDcaNegToPrimVertex      ( 5.     ),
fSibCutInvMassK0s              ( 0.0075 ),

//---> Flags controlling Vertexers
fkRunVertexers    ( kFALSE ),
fkUseLightVertexer ( kTRUE ),
fkDoV0Refit ( kTRUE ),
fkExtraCleanup    ( kTRUE ),

//---> Flag controlling trigger selection
fTrigType(AliVEvent::kMB),

//---> Variables for fTreeEvent
fCentrality(0),
fMVPileupFlag(kFALSE),
fOOBPileupFlag(kFALSE),
fNTOFClusters(-1),
fNTOFMatches(-1),
fNTracksITSsa2010(-1),
fNTracksGlobal2015(-1),
fNTracksGlobal2015TriggerPP(-1),
fAmplitudeV0A(-1.),
fAmplitudeV0C(-1.),
fNHitsFMDA(-1.),
fNHitsFMDC(-1.),

//---> Variables for fTreeV0
fTreeVariableChi2V0(0),
fTreeVariableDcaV0Daughters(0),
fTreeVariableDcaV0ToPrimVertex(0),
fTreeVariableDcaPosToPrimVertex(0),
fTreeVariableDcaNegToPrimVertex(0),
fTreeVariableV0CosineOfPointingAngle(0),
fTreeVariableV0Radius(0),
fTreeVariablePt(0),
fTreeVariablePtMC(0),
fTreeVariableRapK0Short(0),
fTreeVariableRapLambda(0),
fTreeVariableRapMC(0),
fTreeVariableInvMassK0s(0),
fTreeVariableInvMassLambda(0),
fTreeVariableInvMassAntiLambda(0),
fTreeVariableAlphaV0(0),
fTreeVariablePtArmV0(0),
fTreeVariableNegEta(0),
fTreeVariablePosEta(0),

fTreeVariableNSigmasPosProton(0),
fTreeVariableNSigmasPosPion(0),
fTreeVariableNSigmasNegProton(0),
fTreeVariableNSigmasNegPion(0),

fTreeVariableDistOverTotMom(0),
fTreeVariableLeastNbrCrossedRows(0),
fTreeVariableLeastRatioCrossedRowsOverFindable(0),
fTreeVariableMaxChi2PerCluster(0),
fTreeVariableMinTrackLength(0),

fTreeVariablePosPIDForTracking(-1),
fTreeVariableNegPIDForTracking(-1),
fTreeVariablePosdEdx(-1),
fTreeVariableNegdEdx(-1),
fTreeVariablePosInnerP(-1),
fTreeVariableNegInnerP(-1),
fTreeVariableNegTrackStatus(0),
fTreeVariablePosTrackStatus(0),
fTreeVariableNegDCAz(-1),
fTreeVariablePosDCAz(-1),

fTreeVariableNegTOFExpTDiff(99999),
fTreeVariablePosTOFExpTDiff(99999),
fTreeVariableNegTOFSignal(99999),
fTreeVariablePosTOFSignal(99999),
fTreeVariableAmplitudeV0A(-1.),
fTreeVariableAmplitudeV0C(-1.),
fTreeVariableNHitsFMDA(-1.),
fTreeVariableNHitsFMDC(-1.),

fTreeVariableCentrality(0),
fTreeVariableMVPileupFlag(kFALSE),
fTreeVariableOOBPileupFlag(kFALSE),
//MC Variables
fTreeVariablePtMother(0),
fTreeVariableRapMother(0),
fTreeVariablePID(0),
fTreeVariablePIDPositive(0),
fTreeVariablePIDNegative(0),
fTreeVariablePIDMother(0),
fTreeVariablePrimaryStatus(0),
fTreeVariablePrimaryStatusMother(0),

//---> Variables for fTreeCascade
fTreeCascVarCharge(0),
fTreeCascVarMassAsXi(0),
fTreeCascVarMassAsOmega(0),
fTreeCascVarPt(0),
fTreeCascVarPtMC(0),
fTreeCascVarRapXi(0),
fTreeCascVarRapOmega(0),
fTreeCascVarRapMC(0),
fTreeCascVarNegEta(0),
fTreeCascVarPosEta(0),
fTreeCascVarBachEta(0),
fTreeCascVarDCACascDaughters(0),
fTreeCascVarDCABachToPrimVtx(0),
fTreeCascVarDCAV0Daughters(0),
fTreeCascVarDCAV0ToPrimVtx(0),
fTreeCascVarDCAPosToPrimVtx(0),
fTreeCascVarDCANegToPrimVtx(0),
fTreeCascVarCascCosPointingAngle(0),
fTreeCascVarCascDCAtoPVxy(0),
fTreeCascVarCascDCAtoPVz(0),
fTreeCascVarCascRadius(0),
fTreeCascVarV0Mass(0),
fTreeCascVarV0CosPointingAngle(0),
fTreeCascVarV0CosPointingAngleSpecial(0),
fTreeCascVarV0Radius(0),
fTreeCascVarDCABachToBaryon(0),
fTreeCascVarWrongCosPA(0),
fTreeCascVarLeastNbrClusters(0),
fTreeCascVarDistOverTotMom(0),
fTreeCascVarMaxChi2PerCluster(0),
fTreeCascVarMinTrackLength(0),

fTreeCascVarNegNSigmaPion(0),
fTreeCascVarNegNSigmaProton(0),
fTreeCascVarPosNSigmaPion(0),
fTreeCascVarPosNSigmaProton(0),
fTreeCascVarBachNSigmaPion(0),
fTreeCascVarBachNSigmaKaon(0),

fTreeCascVarChiSquareV0(1e+3),
fTreeCascVarChiSquareCascade(1e+3),
fTreeCascVarPosPIDForTracking(-1),
fTreeCascVarNegPIDForTracking(-1),
fTreeCascVarBachPIDForTracking(-1),
fTreeCascVarNegInnerP(-1),
fTreeCascVarPosInnerP(-1),
fTreeCascVarBachInnerP(-1),
fTreeCascVarNegdEdx(-1),
fTreeCascVarPosdEdx(-1),
fTreeCascVarBachdEdx(-1),

fTreeCascVarNegTrackStatus(0), //!
fTreeCascVarPosTrackStatus(0), //!
fTreeCascVarBachTrackStatus(0), //!
fTreeCascVarNegDCAz(-1),
fTreeCascVarPosDCAz(-1),
fTreeCascVarBachDCAz(-1),

//Variables for debugging the invariant mass bump
//Full momentum information
fTreeCascVarNegPx(0),
fTreeCascVarNegPy(0),
fTreeCascVarNegPz(0),
fTreeCascVarPosPx(0),
fTreeCascVarPosPy(0),
fTreeCascVarPosPz(0),
fTreeCascVarBachPx(0),
fTreeCascVarBachPy(0),
fTreeCascVarBachPz(0),
fTreeCascVarNegPxMC(0),
fTreeCascVarNegPyMC(0),
fTreeCascVarNegPzMC(0),
fTreeCascVarPosPxMC(0),
fTreeCascVarPosPyMC(0),
fTreeCascVarPosPzMC(0),
fTreeCascVarBachPxMC(0),
fTreeCascVarBachPyMC(0),
fTreeCascVarBachPzMC(0),
fTreeCascVarV0DecayX(0),
fTreeCascVarV0DecayY(0),
fTreeCascVarV0DecayZ(0),
fTreeCascVarCascadeDecayX(0),
fTreeCascVarCascadeDecayY(0),
fTreeCascVarCascadeDecayZ(0),
fTreeCascVarV0DecayXMC(0),
fTreeCascVarV0DecayYMC(0),
fTreeCascVarV0DecayZMC(0),
fTreeCascVarCascadeDecayXMC(0),
fTreeCascVarCascadeDecayYMC(0),
fTreeCascVarCascadeDecayZMC(0),
fTreeCascVarBachelorDCAptX(0),
fTreeCascVarBachelorDCAptY(0),
fTreeCascVarBachelorDCAptZ(0),
fTreeCascVarV0DCAptX(0),
fTreeCascVarV0DCAptY(0),
fTreeCascVarV0DCAptZ(0),
fTreeCascVarDCADaughters_Test(0),
fTreeCascVarBachelorDCAptSigmaX2(0),
fTreeCascVarBachelorDCAptSigmaY2(0),
fTreeCascVarBachelorDCAptSigmaZ2(0),
fTreeCascVarV0DCAptUncertainty_V0Pos(0),
fTreeCascVarV0DCAptUncertainty_V0Ang(0),
fTreeCascVarV0DCAptPosSigmaX2(0),
fTreeCascVarV0DCAptPosSigmaY2(0),
fTreeCascVarV0DCAptPosSigmaZ2(0),
fTreeCascVarV0DCAptPosSigmaSnp2(0),
fTreeCascVarV0DCAptPosSigmaTgl2(0),
fTreeCascVarV0DCAptNegSigmaX2(0),
fTreeCascVarV0DCAptNegSigmaY2(0),
fTreeCascVarV0DCAptNegSigmaZ2(0),
fTreeCascVarV0DCAptNegSigmaSnp2(0),
fTreeCascVarV0DCAptNegSigmaTgl2(0),
fTreeCascVarBachDCAPVSigmaX2(0),
fTreeCascVarBachDCAPVSigmaY2(0),
fTreeCascVarBachDCAPVSigmaZ2(0),
fTreeCascVarPosDCAPVSigmaX2(0),
fTreeCascVarPosDCAPVSigmaY2(0),
fTreeCascVarPosDCAPVSigmaZ2(0),
fTreeCascVarNegDCAPVSigmaX2(0),
fTreeCascVarNegDCAPVSigmaY2(0),
fTreeCascVarNegDCAPVSigmaZ2(0),
fTreeCascVarPrimVertexX(0),
fTreeCascVarPrimVertexY(0),
fTreeCascVarPrimVertexZ(0),
fTreeCascVarMagField(0),
fTreeCascVarV0Lifetime(0),
fTreeCascVarV0ChiSquare(0),

//Track Labels (check for duplicates, etc)
fTreeCascVarNegIndex(0),
fTreeCascVarPosIndex(0),
fTreeCascVarBachIndex(0),
fTreeCascVarNegLabel(0),
fTreeCascVarPosLabel(0),
fTreeCascVarBachLabel(0),
fTreeCascVarNegLabelMother(0),
fTreeCascVarPosLabelMother(0),
fTreeCascVarBachLabelMother(0),
fTreeCascVarNegLabelGrandMother(0),
fTreeCascVarPosLabelGrandMother(0),
fTreeCascVarBachLabelGrandMother(0),

//Event Number (check same-event index mixups)
fTreeCascVarEventNumber(0),
fTreeCascVarNegTOFExpTDiff(99999),
fTreeCascVarPosTOFExpTDiff(99999),
fTreeCascVarBachTOFExpTDiff(99999),
fTreeCascVarNegTOFSignal(99999),
fTreeCascVarPosTOFSignal(99999),
fTreeCascVarBachTOFSignal(99999),
fTreeCascVarAmplitudeV0A(-1.),
fTreeCascVarAmplitudeV0C(-1.),
fTreeCascVarNHitsFMDA(-1.),
fTreeCascVarNHitsFMDC(-1.),


fTreeCascVarCentrality(0),
fTreeCascVarMVPileupFlag(kFALSE),
fTreeCascVarOOBPileupFlag(kFALSE),

fTreeCascVarPID(0),
fTreeCascVarPIDNegative(0),
fTreeCascVarPIDPositive(0),
fTreeCascVarPIDBachelor(0),
fTreeCascVarPIDNegativeMother(0),
fTreeCascVarPIDPositiveMother(0),
fTreeCascVarPIDBachelorMother(0),
fTreeCascVarPIDNegativeGrandMother(0),
fTreeCascVarPIDPositiveGrandMother(0),
fTreeCascVarPIDBachelorGrandMother(0),

fTreeCascVarBachCousinStatus(0),
fTreeCascVarV0BachSibIsValid(0),
fTreeCascVarBachV0Tagging(0),
fTreeCascVarV0NegSibIsValid(0),
fTreeCascVarNegV0Tagging(0),
fTreeCascVarV0PosSibIsValid(0),
fTreeCascVarPosV0Tagging(0),

fTreeCascVarBachSibPt(0),
fTreeCascVarBachSibDcaV0ToPrimVertex(0),
fTreeCascVarBachSibDcaV0Daughters(0),
fTreeCascVarBachSibV0CosineOfPointingAngle(0),
fTreeCascVarBachSibV0V0Radius(0),
fTreeCascVarBachSibV0DcaPosToPrimVertex(0),
fTreeCascVarBachSibV0DcaNegToPrimVertex(0),
fTreeCascVarBachSibV0InvMassK0s(0),
fTreeCascVarBachSibV0InvMassLambda(0),
fTreeCascVarBachSibV0InvMassAntiLambda(0),

fTreeCascVarNegSibPt(0),
fTreeCascVarNegSibDcaV0ToPrimVertex(0),
fTreeCascVarNegSibDcaV0Daughters(0),
fTreeCascVarNegSibV0CosineOfPointingAngle(0),
fTreeCascVarNegSibV0V0Radius(0),
fTreeCascVarNegSibV0DcaPosToPrimVertex(0),
fTreeCascVarNegSibV0DcaNegToPrimVertex(0),
fTreeCascVarNegSibV0InvMassK0s(0),
fTreeCascVarNegSibV0InvMassLambda(0),
fTreeCascVarNegSibV0InvMassAntiLambda(0),

fTreeCascVarPosSibPt(0),
fTreeCascVarPosSibDcaV0ToPrimVertex(0),
fTreeCascVarPosSibDcaV0Daughters(0),
fTreeCascVarPosSibV0CosineOfPointingAngle(0),
fTreeCascVarPosSibV0V0Radius(0),
fTreeCascVarPosSibV0DcaPosToPrimVertex(0),
fTreeCascVarPosSibV0DcaNegToPrimVertex(0),
fTreeCascVarPosSibV0InvMassK0s(0),
fTreeCascVarPosSibV0InvMassLambda(0),
fTreeCascVarPosSibV0InvMassAntiLambda(0),

fTreeCascVarIsPhysicalPrimary(0),

fTreeCascVarIsPhysicalPrimaryNegative(kFALSE),
fTreeCascVarIsPhysicalPrimaryPositive(kFALSE),
fTreeCascVarIsPhysicalPrimaryBachelor(kFALSE),
fTreeCascVarIsPhysicalPrimaryNegativeMother(kFALSE),
fTreeCascVarIsPhysicalPrimaryPositiveMother(kFALSE),
fTreeCascVarIsPhysicalPrimaryBachelorMother(kFALSE),
fTreeCascVarIsPhysicalPrimaryNegativeGrandMother(kFALSE),
fTreeCascVarIsPhysicalPrimaryPositiveGrandMother(kFALSE),
fTreeCascVarIsPhysicalPrimaryBachelorGrandMother(kFALSE),

fTreeCascVarPosITSClusters0(0),
fTreeCascVarPosITSClusters1(0),
fTreeCascVarPosITSClusters2(0),
fTreeCascVarPosITSClusters3(0),
fTreeCascVarPosITSClusters4(0),
fTreeCascVarPosITSClusters5(0),

fTreeCascVarNegITSClusters0(0),
fTreeCascVarNegITSClusters1(0),
fTreeCascVarNegITSClusters2(0),
fTreeCascVarNegITSClusters3(0),
fTreeCascVarNegITSClusters4(0),
fTreeCascVarNegITSClusters5(0),

fTreeCascVarBachITSClusters0(0),
fTreeCascVarBachITSClusters1(0),
fTreeCascVarBachITSClusters2(0),
fTreeCascVarBachITSClusters3(0),
fTreeCascVarBachITSClusters4(0),
fTreeCascVarBachITSClusters5(0),

fTreeCascVarPosITSSharedClusters0(0),
fTreeCascVarPosITSSharedClusters1(0),
fTreeCascVarPosITSSharedClusters2(0),
fTreeCascVarPosITSSharedClusters3(0),
fTreeCascVarPosITSSharedClusters4(0),
fTreeCascVarPosITSSharedClusters5(0),

fTreeCascVarNegITSSharedClusters0(0),
fTreeCascVarNegITSSharedClusters1(0),
fTreeCascVarNegITSSharedClusters2(0),
fTreeCascVarNegITSSharedClusters3(0),
fTreeCascVarNegITSSharedClusters4(0),
fTreeCascVarNegITSSharedClusters5(0),

fTreeCascVarBachITSSharedClusters0(0),
fTreeCascVarBachITSSharedClusters1(0),
fTreeCascVarBachITSSharedClusters2(0),
fTreeCascVarBachITSSharedClusters3(0),
fTreeCascVarBachITSSharedClusters4(0),
fTreeCascVarBachITSSharedClusters5(0),

//Uncertainty information on mass (from KF) for testing purposes
fTreeCascVarV0LambdaMassError(0),
fTreeCascVarV0AntiLambdaMassError(0),

fTreeCascVarBachIsKink(0),
fTreeCascVarPosIsKink(0),
fTreeCascVarNegIsKink(0),

fTreeCascVarSwappedPID(0),

//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//Save full info for full re-vertex offline replay ('sandbox mode')
//+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
fTreeCascVarBachTrack(0),
fTreeCascVarPosTrack(0),
fTreeCascVarNegTrack(0),
fTreeCascVarMagneticField(0),

//Histos
fHistEventCounter(0),
fHistCentrality(0),
//V0s
fHistGeneratedPtVsYVsCentralityK0Short(0),
fHistGeneratedPtVsYVsCentralityLambda(0),
fHistGeneratedPtVsYVsCentralityAntiLambda(0),
//Cascades
fHistGeneratedPtVsYVsCentralityXiMinus(0),
fHistGeneratedPtVsYVsCentralityXiPlus(0),
fHistGeneratedPtVsYVsCentralityOmegaMinus(0),
fHistGeneratedPtVsYVsCentralityOmegaPlus(0)
//------------------------------------------------
// Tree Variables

{
    
    //Re-vertex: Will only apply for cascade candidates
    
    fV0VertexerSels[0] =  33.  ;  // max allowed chi2
    fV0VertexerSels[1] =   0.02;  // min allowed impact parameter for the 1st daughter (LHC09a4 : 0.05)
    fV0VertexerSels[2] =   0.02;  // min allowed impact parameter for the 2nd daughter (LHC09a4 : 0.05)
    fV0VertexerSels[3] =   2.0 ;  // max allowed DCA between the daughter tracks       (LHC09a4 : 0.5)
    fV0VertexerSels[4] =   0.95;  // min allowed cosine of V0's pointing angle         (LHC09a4 : 0.99)
    fV0VertexerSels[5] =   1.0 ;  // min radius of the fiducial volume                 (LHC09a4 : 0.2)
    fV0VertexerSels[6] = 200.  ;  // max radius of the fiducial volume                 (LHC09a4 : 100.0)
    
    fCascadeVertexerSels[0] =  33.   ;  // max allowed chi2 (same as PDC07)
    fCascadeVertexerSels[1] =   0.05 ;  // min allowed V0 impact parameter                    (PDC07 : 0.05   / LHC09a4 : 0.025 )
    fCascadeVertexerSels[2] =   0.010;  // "window" around the Lambda mass                    (PDC07 : 0.008  / LHC09a4 : 0.010 )
    fCascadeVertexerSels[3] =   0.03 ;  // min allowed bachelor's impact parameter            (PDC07 : 0.035  / LHC09a4 : 0.025 )
    fCascadeVertexerSels[4] =   2.0  ;  // max allowed DCA between the V0 and the bachelor    (PDC07 : 0.1    / LHC09a4 : 0.2   )
    fCascadeVertexerSels[5] =   0.95 ;  // min allowed cosine of the cascade pointing angle   (PDC07 : 0.9985 / LHC09a4 : 0.998 )
    fCascadeVertexerSels[6] =   0.4  ;  // min radius of the fiducial volume                  (PDC07 : 0.9    / LHC09a4 : 0.2   )
    fCascadeVertexerSels[7] = 100.   ;  // max radius of the fiducial volume                  (PDC07 : 100    / LHC09a4 : 100   )
    
    //[0]+[1]*TMath::Exp([2]*x)+[3]*TMath::Exp([4]*x)
    fLambdaMassMean[0]=1.116; //standard fixed
    fLambdaMassMean[1]=0.0;
    fLambdaMassMean[2]=0.0;
    fLambdaMassMean[3]=0.0;
    fLambdaMassMean[4]=0.0;
    
    //[0]+[1]*x+[2]*TMath::Exp([3]*x)
    fLambdaMassSigma[0]=0.002; //standard at roughly the integ val
    fLambdaMassSigma[1]=0.0;
    fLambdaMassSigma[2]=0.0;
    fLambdaMassSigma[3]=0.0;
    
    fkSaveEventTree    = lSaveEventTree;
    fkSaveV0Tree       = lSaveV0Tree;
    fkSaveCascadeTree  = lSaveCascadeTree;
    
    //Standard output
    DefineOutput(1, TList::Class()); // Basic Histograms
    DefineOutput(2, TList::Class()); // V0 Histogram Output
    DefineOutput(3, TList::Class()); // Cascade Histogram Output
    
    //Optional output
    if (fkSaveEventTree)
        DefineOutput(4, TTree::Class()); // Event Tree output
    if (fkSaveV0Tree)
        DefineOutput(5, TTree::Class()); // V0 Tree output
    if (fkSaveCascadeTree)
        DefineOutput(6, TTree::Class()); // Cascade Tree output
    
    //Special Debug Options (more to be added as needed)
    // A - Study Wrong PID for tracking bug
    // B - Study invariant mass *B*ump
    // C - Study OOB pileup in pp 2016 data
    // P - Study *P*arenthood information (for bump, etc)
    // S - Add sandbox mode information, please
    
    if ( lExtraOptions.Contains("A") ) fkDebugWrongPIDForTracking = kTRUE;
    if ( lExtraOptions.Contains("B") ) fkDebugBump                = kTRUE;
    if ( lExtraOptions.Contains("C") ) fkDebugOOBPileup           = kTRUE;
    if ( lExtraOptions.Contains("P") ) fkDebugParenthood          = kTRUE;
    if ( lExtraOptions.Contains("S") ) fkSandboxMode              = kTRUE;
    
}

AliAnalysisTaskStrangenessVsMultiplicityMCRun2::~AliAnalysisTaskStrangenessVsMultiplicityMCRun2()
{
    //------------------------------------------------
    // DESTRUCTOR
    //------------------------------------------------
    
    //Destroy output objects if present
    if (fListHist) {
        delete fListHist;
        fListHist = 0x0;
    }
    if (fListV0) {
        delete fListV0;
        fListV0 = 0x0;
    }
    if (fListCascade) {
        delete fListCascade;
        fListCascade = 0x0;
    }
    if (fTreeEvent) {
        delete fTreeEvent;
        fTreeEvent = 0x0;
    }
    if (fTreeV0) {
        delete fTreeV0;
        fTreeV0 = 0x0;
    }
    if (fTreeCascade) {
        delete fTreeCascade;
        fTreeCascade = 0x0;
    }
    if (fUtils) {
        delete fUtils;
        fUtils = 0x0;
    }
    if (fRand) {
        delete fRand;
        fRand = 0x0;
    }
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityMCRun2::UserCreateOutputObjects()
{
    //------------------------------------------------
    // fTreeEvent: EbyE information
    //------------------------------------------------
    if(fkSaveEventTree){
        fTreeEvent = new TTree("fTreeEvent","Event");
        //Branch Definitions
        fTreeEvent->Branch("fCentrality",&fCentrality,"fCentrality/F");
        fTreeEvent->Branch("fMVPileupFlag",&fMVPileupFlag,"fMVPileupFlag/O");
        //
        if ( fkDebugOOBPileup ){
            fTreeEvent->Branch("fOOBPileupFlag",&fOOBPileupFlag,"fOOBPileupFlag/O");
            fTreeEvent->Branch("fNTOFClusters",&fNTOFClusters,"fNTOFClusters/I");
            fTreeEvent->Branch("fNTOFMatches",&fNTOFMatches,"fNTOFMatches/I");
            fTreeEvent->Branch("fNTracksITSsa2010",&fNTracksITSsa2010,"fNTracksITSsa2010/I");
            fTreeEvent->Branch("fNTracksGlobal2015",&fNTracksGlobal2015,"fNTracksGlobal2015/I");
            fTreeEvent->Branch("fNTracksGlobal2015TriggerPP",&fNTracksGlobal2015TriggerPP,"fNTracksGlobal2015TriggerPP/I");
            fTreeEvent->Branch("fAmplitudeV0A",&fAmplitudeV0A,"fAmplitudeV0A/F");
            fTreeEvent->Branch("fAmplitudeV0C",&fAmplitudeV0C,"fAmplitudeV0C/F");
            fTreeEvent->Branch("fNHitsFMDA",&fNHitsFMDA,"fNHitsFMDA/F");
            fTreeEvent->Branch("fNHitsFMDC",&fNHitsFMDC,"fNHitsFMDC/F");
        }
    }
    
    //------------------------------------------------
    // fTreeV0: V0 Candidate Information
    //------------------------------------------------
    if(fkSaveV0Tree){
        //Create Basic V0 Output Tree
        fTreeV0 = new TTree( "fTreeV0", "V0 Candidates");
        //-----------BASIC-INFO---------------------------
        fTreeV0->Branch("fTreeVariableChi2V0",&fTreeVariableChi2V0,"fTreeVariableChi2V0/F");
        fTreeV0->Branch("fTreeVariableDcaV0Daughters",&fTreeVariableDcaV0Daughters,"fTreeVariableDcaV0Daughters/F");
        fTreeV0->Branch("fTreeVariableDcaV0ToPrimVertex",&fTreeVariableDcaV0ToPrimVertex,"fTreeVariableDcaV0ToPrimVertex/F");
        fTreeV0->Branch("fTreeVariableDcaPosToPrimVertex",&fTreeVariableDcaPosToPrimVertex,"fTreeVariableDcaPosToPrimVertex/F");
        fTreeV0->Branch("fTreeVariableDcaNegToPrimVertex",&fTreeVariableDcaNegToPrimVertex,"fTreeVariableDcaNegToPrimVertex/F");
        fTreeV0->Branch("fTreeVariableV0Radius",&fTreeVariableV0Radius,"fTreeVariableV0Radius/F");
        fTreeV0->Branch("fTreeVariablePt",&fTreeVariablePt,"fTreeVariablePt/F");
        fTreeV0->Branch("fTreeVariablePtMC",&fTreeVariablePtMC,"fTreeVariablePtMC/F");
        fTreeV0->Branch("fTreeVariableRapK0Short",&fTreeVariableRapK0Short,"fTreeVariableRapK0Short/F");
        fTreeV0->Branch("fTreeVariableRapLambda",&fTreeVariableRapLambda,"fTreeVariableRapLambda/F");
        fTreeV0->Branch("fTreeVariableRapMC",&fTreeVariableRapMC,"fTreeVariableRapMC/F");
        fTreeV0->Branch("fTreeVariableInvMassK0s",&fTreeVariableInvMassK0s,"fTreeVariableInvMassK0s/F");
        fTreeV0->Branch("fTreeVariableInvMassLambda",&fTreeVariableInvMassLambda,"fTreeVariableInvMassLambda/F");
        fTreeV0->Branch("fTreeVariableInvMassAntiLambda",&fTreeVariableInvMassAntiLambda,"fTreeVariableInvMassAntiLambda/F");
        fTreeV0->Branch("fTreeVariableV0CosineOfPointingAngle",&fTreeVariableV0CosineOfPointingAngle,"fTreeVariableV0CosineOfPointingAngle/F");
        fTreeV0->Branch("fTreeVariableAlphaV0",&fTreeVariableAlphaV0,"fTreeVariableAlphaV0/F");
        fTreeV0->Branch("fTreeVariablePtArmV0",&fTreeVariablePtArmV0,"fTreeVariablePtArmV0/F");
        fTreeV0->Branch("fTreeVariableLeastNbrCrossedRows",&fTreeVariableLeastNbrCrossedRows,"fTreeVariableLeastNbrCrossedRows/I");
        fTreeV0->Branch("fTreeVariableLeastRatioCrossedRowsOverFindable",&fTreeVariableLeastRatioCrossedRowsOverFindable,"fTreeVariableLeastRatioCrossedRowsOverFindable/F");
        fTreeV0->Branch("fTreeVariableMaxChi2PerCluster",&fTreeVariableMaxChi2PerCluster,"fTreeVariableMaxChi2PerCluster/F");
        fTreeV0->Branch("fTreeVariableMinTrackLength",&fTreeVariableMinTrackLength,"fTreeVariableMinTrackLength/F");
        fTreeV0->Branch("fTreeVariableDistOverTotMom",&fTreeVariableDistOverTotMom,"fTreeVariableDistOverTotMom/F");
        fTreeV0->Branch("fTreeVariableNSigmasPosProton",&fTreeVariableNSigmasPosProton,"fTreeVariableNSigmasPosProton/F");
        fTreeV0->Branch("fTreeVariableNSigmasPosPion",&fTreeVariableNSigmasPosPion,"fTreeVariableNSigmasPosPion/F");
        fTreeV0->Branch("fTreeVariableNSigmasNegProton",&fTreeVariableNSigmasNegProton,"fTreeVariableNSigmasNegProton/F");
        fTreeV0->Branch("fTreeVariableNSigmasNegPion",&fTreeVariableNSigmasNegPion,"fTreeVariableNSigmasNegPion/F");
        fTreeV0->Branch("fTreeVariableNegEta",&fTreeVariableNegEta,"fTreeVariableNegEta/F");
        fTreeV0->Branch("fTreeVariablePosEta",&fTreeVariablePosEta,"fTreeVariablePosEta/F");
        //-----------MULTIPLICITY-INFO--------------------
        fTreeV0->Branch("fTreeVariableCentrality",&fTreeVariableCentrality,"fTreeVariableCentrality/F");
        fTreeV0->Branch("fTreeVariableMVPileupFlag",&fTreeVariableMVPileupFlag,"fTreeVariableMVPileupFlag/O");
        //------------------------------------------------
        if ( fkDebugWrongPIDForTracking ){
            fTreeV0->Branch("fTreeVariablePosPIDForTracking",&fTreeVariablePosPIDForTracking,"fTreeVariablePosPIDForTracking/I");
            fTreeV0->Branch("fTreeVariableNegPIDForTracking",&fTreeVariableNegPIDForTracking,"fTreeVariableNegPIDForTracking/I");
            fTreeV0->Branch("fTreeVariablePosdEdx",&fTreeVariablePosdEdx,"fTreeVariablePosdEdx/F");
            fTreeV0->Branch("fTreeVariableNegdEdx",&fTreeVariableNegdEdx,"fTreeVariableNegdEdx/F");
            fTreeV0->Branch("fTreeVariablePosInnerP",&fTreeVariablePosInnerP,"fTreeVariablePosInnerP/F");
            fTreeV0->Branch("fTreeVariableNegInnerP",&fTreeVariableNegInnerP,"fTreeVariableNegInnerP/F");
            fTreeV0->Branch("fTreeVariableNegTrackStatus",&fTreeVariableNegTrackStatus,"fTreeVariableNegTrackStatus/l");
            fTreeV0->Branch("fTreeVariablePosTrackStatus",&fTreeVariablePosTrackStatus,"fTreeVariablePosTrackStatus/l");
            fTreeV0->Branch("fTreeVariableNegDCAz",&fTreeVariableNegDCAz,"fTreeVariableNegDCAz/F");
            fTreeV0->Branch("fTreeVariablePosDCAz",&fTreeVariablePosDCAz,"fTreeVariablePosDCAz/F");
        }
        if ( fkDebugOOBPileup ) {
            fTreeV0->Branch("fTreeVariableNegTOFExpTDiff",&fTreeVariableNegTOFExpTDiff,"fTreeVariableNegTOFExpTDiff/F");
            fTreeV0->Branch("fTreeVariablePosTOFExpTDiff",&fTreeVariablePosTOFExpTDiff,"fTreeVariablePosTOFExpTDiff/F");
            fTreeV0->Branch("fTreeVariableNegTOFSignal",&fTreeVariableNegTOFSignal,"fTreeVariableNegTOFSignal/F");
            fTreeV0->Branch("fTreeVariablePosTOFSignal",&fTreeVariablePosTOFSignal,"fTreeVariablePosTOFSignal/F");
            // Event info
            fTreeV0->Branch("fTreeVariableOOBPileupFlag",&fTreeVariableOOBPileupFlag,"fTreeVariableOOBPileupFlag/O");
            fTreeV0->Branch("fTreeVariableAmplitudeV0A",&fTreeVariableAmplitudeV0A,"fTreeVariableAmplitudeV0A/F");
            fTreeV0->Branch("fTreeVariableAmplitudeV0C",&fTreeVariableAmplitudeV0C,"fTreeVariableAmplitudeV0C/F");
            fTreeV0->Branch("fTreeVariableNHitsFMDA",&fTreeVariableNHitsFMDA,"fTreeVariableNHitsFMDA/F");
            fTreeV0->Branch("fTreeVariableNHitsFMDC",&fTreeVariableNHitsFMDC,"fTreeVariableNHitsFMDC/F");
        }
        //-----------MC Exclusive info--------------------
        fTreeV0->Branch("fTreeVariablePtMother",&fTreeVariablePtMother,"fTreeVariablePtMother/F");
        fTreeV0->Branch("fTreeVariableRapMother",&fTreeVariableRapMother,"fTreeVariableRapMother/F");
        fTreeV0->Branch("fTreeVariablePID",&fTreeVariablePID,"fTreeVariablePID/I");
        fTreeV0->Branch("fTreeVariablePIDPositive",&fTreeVariablePIDPositive,"fTreeVariablePIDPositive/I");
        fTreeV0->Branch("fTreeVariablePIDNegative",&fTreeVariablePIDNegative,"fTreeVariablePIDNegative/I");
        fTreeV0->Branch("fTreeVariablePIDMother",&fTreeVariablePIDMother,"fTreeVariablePIDMother/I");
        fTreeV0->Branch("fTreeVariablePrimaryStatus",&fTreeVariablePrimaryStatus,"fTreeVariablePrimaryStatus/I");
        fTreeV0->Branch("fTreeVariablePrimaryStatusMother",&fTreeVariablePrimaryStatusMother,"fTreeVariablePrimaryStatusMother/I");
        //------------------------------------------------
    }
    
    //------------------------------------------------
    // fTreeCascade Branch definitions - Cascade Tree
    //------------------------------------------------
    if(fkSaveCascadeTree){
        //Create Cascade output tree
        fTreeCascade = new TTree("fTreeCascade","CascadeCandidates");
        //-----------BASIC-INFO---------------------------
        fTreeCascade->Branch("fTreeCascVarCharge",&fTreeCascVarCharge,"fTreeCascVarCharge/I");
        fTreeCascade->Branch("fTreeCascVarMassAsXi",&fTreeCascVarMassAsXi,"fTreeCascVarMassAsXi/F");
        fTreeCascade->Branch("fTreeCascVarMassAsOmega",&fTreeCascVarMassAsOmega,"fTreeCascVarMassAsOmega/F");
        fTreeCascade->Branch("fTreeCascVarPt",&fTreeCascVarPt,"fTreeCascVarPt/F");
        fTreeCascade->Branch("fTreeCascVarPtMC",&fTreeCascVarPtMC,"fTreeCascVarPtMC/F");
        fTreeCascade->Branch("fTreeCascVarRapXi",&fTreeCascVarRapXi,"fTreeCascVarRapXi/F");
        fTreeCascade->Branch("fTreeCascVarRapOmega",&fTreeCascVarRapOmega,"fTreeCascVarRapOmega/F");
        fTreeCascade->Branch("fTreeCascVarRapMC",&fTreeCascVarRapMC,"fTreeCascVarRapMC/F");
        fTreeCascade->Branch("fTreeCascVarNegEta",&fTreeCascVarNegEta,"fTreeCascVarNegEta/F");
        fTreeCascade->Branch("fTreeCascVarPosEta",&fTreeCascVarPosEta,"fTreeCascVarPosEta/F");
        fTreeCascade->Branch("fTreeCascVarBachEta",&fTreeCascVarBachEta,"fTreeCascVarBachEta/F");
        //-----------INFO-FOR-CUTS------------------------
        fTreeCascade->Branch("fTreeCascVarDCACascDaughters",&fTreeCascVarDCACascDaughters,"fTreeCascVarDCACascDaughters/F");
        fTreeCascade->Branch("fTreeCascVarDCABachToPrimVtx",&fTreeCascVarDCABachToPrimVtx,"fTreeCascVarDCABachToPrimVtx/F");
        fTreeCascade->Branch("fTreeCascVarDCAV0Daughters",&fTreeCascVarDCAV0Daughters,"fTreeCascVarDCAV0Daughters/F");
        fTreeCascade->Branch("fTreeCascVarDCAV0ToPrimVtx",&fTreeCascVarDCAV0ToPrimVtx,"fTreeCascVarDCAV0ToPrimVtx/F");
        fTreeCascade->Branch("fTreeCascVarDCAPosToPrimVtx",&fTreeCascVarDCAPosToPrimVtx,"fTreeCascVarDCAPosToPrimVtx/F");
        fTreeCascade->Branch("fTreeCascVarDCANegToPrimVtx",&fTreeCascVarDCANegToPrimVtx,"fTreeCascVarDCANegToPrimVtx/F");
        fTreeCascade->Branch("fTreeCascVarCascCosPointingAngle",&fTreeCascVarCascCosPointingAngle,"fTreeCascVarCascCosPointingAngle/F");
        fTreeCascade->Branch("fTreeCascVarCascDCAtoPVxy",&fTreeCascVarCascDCAtoPVxy,"fTreeCascVarCascDCAtoPVxy/F");
        fTreeCascade->Branch("fTreeCascVarCascDCAtoPVz",&fTreeCascVarCascDCAtoPVz,"fTreeCascVarCascDCAtoPVz/F");
        
        fTreeCascade->Branch("fTreeCascVarCascRadius",&fTreeCascVarCascRadius,"fTreeCascVarCascRadius/F");
        fTreeCascade->Branch("fTreeCascVarV0Mass",&fTreeCascVarV0Mass,"fTreeCascVarV0Mass/F");
        fTreeCascade->Branch("fTreeCascVarV0CosPointingAngle",&fTreeCascVarV0CosPointingAngle,"fTreeCascVarV0CosPointingAngle/F");
        fTreeCascade->Branch("fTreeCascVarV0CosPointingAngleSpecial",&fTreeCascVarV0CosPointingAngleSpecial,"fTreeCascVarV0CosPointingAngleSpecial/F");
        fTreeCascade->Branch("fTreeCascVarV0Radius",&fTreeCascVarV0Radius,"fTreeCascVarV0Radius/F");
        fTreeCascade->Branch("fTreeCascVarDCABachToBaryon",&fTreeCascVarDCABachToBaryon,"fTreeCascVarDCABachToBaryon/F");
        fTreeCascade->Branch("fTreeCascVarWrongCosPA",&fTreeCascVarWrongCosPA,"fTreeCascVarWrongCosPA/F");
        fTreeCascade->Branch("fTreeCascVarLeastNbrClusters",&fTreeCascVarLeastNbrClusters,"fTreeCascVarLeastNbrClusters/I");
        fTreeCascade->Branch("fTreeCascVarMaxChi2PerCluster",&fTreeCascVarMaxChi2PerCluster,"fTreeCascVarMaxChi2PerCluster/F");
        fTreeCascade->Branch("fTreeCascVarMinTrackLength",&fTreeCascVarMinTrackLength,"fTreeCascVarMinTrackLength/F");
        //-----------MULTIPLICITY-INFO--------------------
        fTreeCascade->Branch("fTreeCascVarCentrality",&fTreeCascVarCentrality,"fTreeCascVarCentrality/F");
        fTreeCascade->Branch("fTreeCascVarMVPileupFlag",&fTreeCascVarMVPileupFlag,"fTreeCascVarMVPileupFlag/O");
        //-----------DECAY-LENGTH-INFO--------------------
        fTreeCascade->Branch("fTreeCascVarDistOverTotMom",&fTreeCascVarDistOverTotMom,"fTreeCascVarDistOverTotMom/F");
        //------------------------------------------------
        fTreeCascade->Branch("fTreeCascVarNegNSigmaPion",&fTreeCascVarNegNSigmaPion,"fTreeCascVarNegNSigmaPion/F");
        fTreeCascade->Branch("fTreeCascVarNegNSigmaProton",&fTreeCascVarNegNSigmaProton,"fTreeCascVarNegNSigmaProton/F");
        fTreeCascade->Branch("fTreeCascVarPosNSigmaPion",&fTreeCascVarPosNSigmaPion,"fTreeCascVarPosNSigmaPion/F");
        fTreeCascade->Branch("fTreeCascVarPosNSigmaProton",&fTreeCascVarPosNSigmaProton,"fTreeCascVarPosNSigmaProton/F");
        fTreeCascade->Branch("fTreeCascVarBachNSigmaPion",&fTreeCascVarBachNSigmaPion,"fTreeCascVarBachNSigmaPion/F");
        fTreeCascade->Branch("fTreeCascVarBachNSigmaKaon",&fTreeCascVarBachNSigmaKaon,"fTreeCascVarBachNSigmaKaon/F");
        //------------------------------------------------
        fTreeCascade->Branch("fTreeCascVarChiSquareV0",&fTreeCascVarChiSquareV0,"fTreeCascVarChiSquareV0/F");
        fTreeCascade->Branch("fTreeCascVarChiSquareCascade",&fTreeCascVarChiSquareCascade,"fTreeCascVarChiSquareCascade/F");
        //------------------------------------------------
        //Variables for test with bachelor sibling V0
        //Bach
        fTreeCascade->Branch("fTreeCascVarBachSibPt",&fTreeCascVarBachSibPt," fTreeCascVarBachSibPt/F");
        fTreeCascade->Branch("fTreeCascVarBachSibDcaV0ToPrimVertex",&fTreeCascVarBachSibDcaV0ToPrimVertex," fTreeCascVarBachSibDcaV0ToPrimVertex/F");
        fTreeCascade->Branch("fTreeCascVarBachSibDcaV0Daughters",&fTreeCascVarBachSibDcaV0Daughters," fTreeCascVarBachSibDcaV0Daughters/F");
        fTreeCascade->Branch("fTreeCascVarBachSibV0CosineOfPointingAngle",&fTreeCascVarBachSibV0CosineOfPointingAngle," fTreeCascVarBachSibV0CosineOfPointingAngle /F");
        fTreeCascade->Branch("fTreeCascVarBachSibV0V0Radius",&fTreeCascVarBachSibV0V0Radius," fTreeCascVarBachSibV0V0Radius/F");
        fTreeCascade->Branch("fTreeCascVarBachSibV0DcaPosToPrimVertex",&fTreeCascVarBachSibV0DcaPosToPrimVertex," fTreeCascVarBachSibV0DcaPosToPrimVertex/F");
        fTreeCascade->Branch("fTreeCascVarBachSibV0DcaNegToPrimVertex",&fTreeCascVarBachSibV0DcaNegToPrimVertex," fTreeCascVarBachSibV0DcaNegToPrimVertex/F");
        fTreeCascade->Branch("fTreeCascVarBachSibV0InvMassK0s",&fTreeCascVarBachSibV0InvMassK0s," fTreeCascVarBachSibV0InvMassK0s            /F");
        fTreeCascade->Branch("fTreeCascVarBachSibV0InvMassLambda",&fTreeCascVarBachSibV0InvMassLambda," fTreeCascVarBachSibV0InvMassLambda/F");
        fTreeCascade->Branch("fTreeCascVarBachSibV0InvMassAntiLambda",&fTreeCascVarBachSibV0InvMassAntiLambda," fTreeCascVarBachSibV0InvMassAntiLambda/F");

        //Neg
        fTreeCascade->Branch("fTreeCascVarNegSibPt",&fTreeCascVarNegSibPt," fTreeCascVarNegSibPt/F");
        fTreeCascade->Branch("fTreeCascVarNegSibDcaV0ToPrimVertex",&fTreeCascVarNegSibDcaV0ToPrimVertex," fTreeCascVarNegSibDcaV0ToPrimVertex/F");
        fTreeCascade->Branch("fTreeCascVarNegSibDcaV0Daughters",&fTreeCascVarNegSibDcaV0Daughters," fTreeCascVarNegSibDcaV0Daughters/F");
        fTreeCascade->Branch("fTreeCascVarNegSibV0CosineOfPointingAngle",&fTreeCascVarNegSibV0CosineOfPointingAngle," fTreeCascVarNegSibV0CosineOfPointingAngle /F");
        fTreeCascade->Branch("fTreeCascVarNegSibV0V0Radius",&fTreeCascVarNegSibV0V0Radius," fTreeCascVarNegSibV0V0Radius/F");
        fTreeCascade->Branch("fTreeCascVarNegSibV0DcaPosToPrimVertex",&fTreeCascVarNegSibV0DcaPosToPrimVertex," fTreeCascVarNegSibV0DcaPosToPrimVertex/F");
        fTreeCascade->Branch("fTreeCascVarNegSibV0DcaNegToPrimVertex",&fTreeCascVarNegSibV0DcaNegToPrimVertex," fTreeCascVarNegSibV0DcaNegToPrimVertex/F");
        fTreeCascade->Branch("fTreeCascVarNegSibV0InvMassK0s",&fTreeCascVarNegSibV0InvMassK0s," fTreeCascVarNegSibV0InvMassK0s            /F");
        fTreeCascade->Branch("fTreeCascVarNegSibV0InvMassLambda",&fTreeCascVarNegSibV0InvMassLambda," fTreeCascVarNegSibV0InvMassLambda/F");
        fTreeCascade->Branch("fTreeCascVarNegSibV0InvMassAntiLambda",&fTreeCascVarNegSibV0InvMassAntiLambda," fTreeCascVarNegSibV0InvMassAntiLambda/F");

        //Pos
        fTreeCascade->Branch("fTreeCascVarPosSibPt",&fTreeCascVarPosSibPt," fTreeCascVarPosSibPt/F");
        fTreeCascade->Branch("fTreeCascVarPosSibDcaV0ToPrimVertex",&fTreeCascVarPosSibDcaV0ToPrimVertex," fTreeCascVarPosSibDcaV0ToPrimVertex/F");
        fTreeCascade->Branch("fTreeCascVarPosSibDcaV0Daughters",&fTreeCascVarPosSibDcaV0Daughters," fTreeCascVarPosSibDcaV0Daughters/F");
        fTreeCascade->Branch("fTreeCascVarPosSibV0CosineOfPointingAngle",&fTreeCascVarPosSibV0CosineOfPointingAngle," fTreeCascVarPosSibV0CosineOfPointingAngle /F");
        fTreeCascade->Branch("fTreeCascVarPosSibV0V0Radius",&fTreeCascVarPosSibV0V0Radius," fTreeCascVarPosSibV0V0Radius/F");
        fTreeCascade->Branch("fTreeCascVarPosSibV0DcaPosToPrimVertex",&fTreeCascVarPosSibV0DcaPosToPrimVertex," fTreeCascVarPosSibV0DcaPosToPrimVertex/F");
        fTreeCascade->Branch("fTreeCascVarPosSibV0DcaNegToPrimVertex",&fTreeCascVarPosSibV0DcaNegToPrimVertex," fTreeCascVarPosSibV0DcaNegToPrimVertex/F");
        fTreeCascade->Branch("fTreeCascVarPosSibV0InvMassK0s",&fTreeCascVarPosSibV0InvMassK0s," fTreeCascVarPosSibV0InvMassK0s            /F");
        fTreeCascade->Branch("fTreeCascVarPosSibV0InvMassLambda",&fTreeCascVarPosSibV0InvMassLambda," fTreeCascVarPosSibV0InvMassLambda/F");
        fTreeCascade->Branch("fTreeCascVarPosSibV0InvMassAntiLambda",&fTreeCascVarPosSibV0InvMassAntiLambda," fTreeCascVarPosSibV0InvMassAntiLambda/F");

        if ( fkDebugWrongPIDForTracking ){
            fTreeCascade->Branch("fTreeCascVarPosPIDForTracking",&fTreeCascVarPosPIDForTracking,"fTreeCascVarPosPIDForTracking/I");
            fTreeCascade->Branch("fTreeCascVarNegPIDForTracking",&fTreeCascVarNegPIDForTracking,"fTreeCascVarNegPIDForTracking/I");
            fTreeCascade->Branch("fTreeCascVarBachPIDForTracking",&fTreeCascVarBachPIDForTracking,"fTreeCascVarBachPIDForTracking/I");
            fTreeCascade->Branch("fTreeCascVarPosdEdx",&fTreeCascVarPosdEdx,"fTreeCascVarPosdEdx/F");
            fTreeCascade->Branch("fTreeCascVarNegdEdx",&fTreeCascVarNegdEdx,"fTreeCascVarNegdEdx/F");
            fTreeCascade->Branch("fTreeCascVarBachdEdx",&fTreeCascVarBachdEdx,"fTreeCascVarBachdEdx/F");
            fTreeCascade->Branch("fTreeCascVarPosInnerP",&fTreeCascVarPosInnerP,"fTreeCascVarPosInnerP/F");
            fTreeCascade->Branch("fTreeCascVarNegInnerP",&fTreeCascVarNegInnerP,"fTreeCascVarNegInnerP/F");
            fTreeCascade->Branch("fTreeCascVarBachInnerP",&fTreeCascVarBachInnerP,"fTreeCascVarBachInnerP/F");
            fTreeCascade->Branch("fTreeCascVarNegTrackStatus",&fTreeCascVarNegTrackStatus,"fTreeCascVarNegTrackStatus/l");
            fTreeCascade->Branch("fTreeCascVarPosTrackStatus",&fTreeCascVarPosTrackStatus,"fTreeCascVarPosTrackStatus/l");
            fTreeCascade->Branch("fTreeCascVarBachTrackStatus",&fTreeCascVarBachTrackStatus,"fTreeCascVarBachTrackStatus/l");
            fTreeCascade->Branch("fTreeCascVarNegDCAz",&fTreeCascVarNegDCAz,"fTreeCascVarNegDCAz/F");
            fTreeCascade->Branch("fTreeCascVarPosDCAz",&fTreeCascVarPosDCAz,"fTreeCascVarPosDCAz/F");
            fTreeCascade->Branch("fTreeCascVarBachDCAz",&fTreeCascVarBachDCAz,"fTreeCascVarBachDCAz/F");
        }
        //------------------------------------------------
        if ( fkDebugBump ){
            //Variables for debugging the invariant mass bump
            
            fTreeCascade->Branch("fTreeCascVarPosPx",&fTreeCascVarPosPx,"fTreeCascVarPosPx/F");
            fTreeCascade->Branch("fTreeCascVarPosPy",&fTreeCascVarPosPy,"fTreeCascVarPosPy/F");
            fTreeCascade->Branch("fTreeCascVarPosPz",&fTreeCascVarPosPz,"fTreeCascVarPosPz/F");
            fTreeCascade->Branch("fTreeCascVarNegPx",&fTreeCascVarNegPx,"fTreeCascVarNegPx/F");
            fTreeCascade->Branch("fTreeCascVarNegPy",&fTreeCascVarNegPy,"fTreeCascVarNegPy/F");
            fTreeCascade->Branch("fTreeCascVarNegPz",&fTreeCascVarNegPz,"fTreeCascVarNegPz/F");
            fTreeCascade->Branch("fTreeCascVarBachPx",&fTreeCascVarBachPx,"fTreeCascVarBachPx/F");
            fTreeCascade->Branch("fTreeCascVarBachPy",&fTreeCascVarBachPy,"fTreeCascVarBachPy/F");
            fTreeCascade->Branch("fTreeCascVarBachPz",&fTreeCascVarBachPz,"fTreeCascVarBachPz/F");
            fTreeCascade->Branch("fTreeCascVarPosPxMC",&fTreeCascVarPosPxMC,"fTreeCascVarPosPxMC/F");
            fTreeCascade->Branch("fTreeCascVarPosPyMC",&fTreeCascVarPosPyMC,"fTreeCascVarPosPyMC/F");
            fTreeCascade->Branch("fTreeCascVarPosPzMC",&fTreeCascVarPosPzMC,"fTreeCascVarPosPzMC/F");
            fTreeCascade->Branch("fTreeCascVarNegPxMC",&fTreeCascVarNegPxMC,"fTreeCascVarNegPxMC/F");
            fTreeCascade->Branch("fTreeCascVarNegPyMC",&fTreeCascVarNegPyMC,"fTreeCascVarNegPyMC/F");
            fTreeCascade->Branch("fTreeCascVarNegPzMC",&fTreeCascVarNegPzMC,"fTreeCascVarNegPzMC/F");
            fTreeCascade->Branch("fTreeCascVarBachPxMC",&fTreeCascVarBachPxMC,"fTreeCascVarBachPxMC/F");
            fTreeCascade->Branch("fTreeCascVarBachPyMC",&fTreeCascVarBachPyMC,"fTreeCascVarBachPyMC/F");
            fTreeCascade->Branch("fTreeCascVarBachPzMC",&fTreeCascVarBachPzMC,"fTreeCascVarBachPzMC/F");
            // Decay positions
            fTreeCascade->Branch("fTreeCascVarV0DecayX",&fTreeCascVarV0DecayX,"fTreeCascVarV0DecayX/F");
            fTreeCascade->Branch("fTreeCascVarV0DecayY",&fTreeCascVarV0DecayY,"fTreeCascVarV0DecayY/F");
            fTreeCascade->Branch("fTreeCascVarV0DecayZ",&fTreeCascVarV0DecayZ,"fTreeCascVarV0DecayZ/F");
            fTreeCascade->Branch("fTreeCascVarCascadeDecayX",&fTreeCascVarCascadeDecayX,"fTreeCascVarCascadeDecayX/F");
            fTreeCascade->Branch("fTreeCascVarCascadeDecayY",&fTreeCascVarCascadeDecayY,"fTreeCascVarCascadeDecayY/F");
            fTreeCascade->Branch("fTreeCascVarCascadeDecayZ",&fTreeCascVarCascadeDecayZ,"fTreeCascVarCascadeDecayZ/F");
            // MC record decay positions
            fTreeCascade->Branch("fTreeCascVarV0DecayXMC",&fTreeCascVarV0DecayXMC,"fTreeCascVarV0DecayXMC/F");
            fTreeCascade->Branch("fTreeCascVarV0DecayYMC",&fTreeCascVarV0DecayYMC,"fTreeCascVarV0DecayYMC/F");
            fTreeCascade->Branch("fTreeCascVarV0DecayZMC",&fTreeCascVarV0DecayZMC,"fTreeCascVarV0DecayZMC/F");
            fTreeCascade->Branch("fTreeCascVarCascadeDecayXMC",&fTreeCascVarCascadeDecayXMC,"fTreeCascVarCascadeDecayXMC/F");
            fTreeCascade->Branch("fTreeCascVarCascadeDecayYMC",&fTreeCascVarCascadeDecayYMC,"fTreeCascVarCascadeDecayYMC/F");
            fTreeCascade->Branch("fTreeCascVarCascadeDecayZMC",&fTreeCascVarCascadeDecayZMC,"fTreeCascVarCascadeDecayZMC/F");
            
            fTreeCascade->Branch("fTreeCascVarV0Lifetime",&fTreeCascVarV0Lifetime,"fTreeCascVarV0Lifetime/F");
            fTreeCascade->Branch("fTreeCascVarV0ChiSquare",&fTreeCascVarV0ChiSquare,"fTreeCascVarV0ChiSquare/F");
            fTreeCascade->Branch("fTreeCascVarMagField",&fTreeCascVarMagField,"fTreeCascVarMagField/F");
            //Track Labels (check for duplicates, etc)
            
            fTreeCascade->Branch("fTreeCascVarBachelorDCAptX",&fTreeCascVarBachelorDCAptX,"fTreeCascVarBachelorDCAptX/F");
            fTreeCascade->Branch("fTreeCascVarBachelorDCAptY",&fTreeCascVarBachelorDCAptY,"fTreeCascVarBachelorDCAptY/F");
            fTreeCascade->Branch("fTreeCascVarBachelorDCAptZ",&fTreeCascVarBachelorDCAptZ,"fTreeCascVarBachelorDCAptZ/F");
            fTreeCascade->Branch("fTreeCascVarV0DCAptX",&fTreeCascVarV0DCAptX,"fTreeCascVarV0DCAptX/F");
            fTreeCascade->Branch("fTreeCascVarV0DCAptY",&fTreeCascVarV0DCAptY,"fTreeCascVarV0DCAptY/F");
            fTreeCascade->Branch("fTreeCascVarV0DCAptZ",&fTreeCascVarV0DCAptZ,"fTreeCascVarV0DCAptZ/F");
            fTreeCascade->Branch("fTreeCascVarDCADaughters_Test",&fTreeCascVarDCADaughters_Test,"fTreeCascVarDCADaughters_Test/F");
            fTreeCascade->Branch("fTreeCascVarBachelorDCAptSigmaX2",&fTreeCascVarBachelorDCAptSigmaX2,"fTreeCascVarBachelorDCAptSigmaX2/F");
            fTreeCascade->Branch("fTreeCascVarBachelorDCAptSigmaY2",&fTreeCascVarBachelorDCAptSigmaY2,"fTreeCascVarBachelorDCAptSigmaY2/F");
            fTreeCascade->Branch("fTreeCascVarBachelorDCAptSigmaZ2",&fTreeCascVarBachelorDCAptSigmaZ2,"fTreeCascVarBachelorDCAptSigmaZ2/F");
            fTreeCascade->Branch("fTreeCascVarV0DCAptUncertainty_V0Pos",&fTreeCascVarV0DCAptUncertainty_V0Pos,"fTreeCascVarV0DCAptUncertainty_V0Pos/F");
            fTreeCascade->Branch("fTreeCascVarV0DCAptUncertainty_V0Ang",&fTreeCascVarV0DCAptUncertainty_V0Ang,"fTreeCascVarV0DCAptUncertainty_V0Ang/F");
            
            fTreeCascade->Branch("fTreeCascVarV0DCAptPosSigmaX2",&fTreeCascVarV0DCAptPosSigmaX2,"fTreeCascVarV0DCAptPosSigmaX2/F");
            fTreeCascade->Branch("fTreeCascVarV0DCAptPosSigmaY2",&fTreeCascVarV0DCAptPosSigmaY2,"fTreeCascVarV0DCAptPosSigmaY2/F");
            fTreeCascade->Branch("fTreeCascVarV0DCAptPosSigmaZ2",&fTreeCascVarV0DCAptPosSigmaZ2,"fTreeCascVarV0DCAptPosSigmaZ2/F");
            fTreeCascade->Branch("fTreeCascVarV0DCAptPosSigmaSnp2",&fTreeCascVarV0DCAptPosSigmaSnp2,"fTreeCascVarV0DCAptPosSigmaSnp2/F");
            fTreeCascade->Branch("fTreeCascVarV0DCAptPosSigmaTgl2",&fTreeCascVarV0DCAptPosSigmaTgl2,"fTreeCascVarV0DCAptPosSigmaTgl2/F");
            
            fTreeCascade->Branch("fTreeCascVarV0DCAptNegSigmaX2",&fTreeCascVarV0DCAptNegSigmaX2,"fTreeCascVarV0DCAptNegSigmaX2/F");
            fTreeCascade->Branch("fTreeCascVarV0DCAptNegSigmaY2",&fTreeCascVarV0DCAptNegSigmaY2,"fTreeCascVarV0DCAptNegSigmaY2/F");
            fTreeCascade->Branch("fTreeCascVarV0DCAptNegSigmaZ2",&fTreeCascVarV0DCAptNegSigmaZ2,"fTreeCascVarV0DCAptNegSigmaZ2/F");
            fTreeCascade->Branch("fTreeCascVarV0DCAptNegSigmaSnp2",&fTreeCascVarV0DCAptNegSigmaSnp2,"fTreeCascVarV0DCAptNegSigmaSnp2/F");
            fTreeCascade->Branch("fTreeCascVarV0DCAptNegSigmaTgl2",&fTreeCascVarV0DCAptNegSigmaTgl2,"fTreeCascVarV0DCAptNegSigmaTgl2/F");
            fTreeCascade->Branch("fTreeCascVarNegDCAPVSigmaX2",&fTreeCascVarNegDCAPVSigmaX2,"fTreeCascVarNegDCAPVSigmaX2/F");
            fTreeCascade->Branch("fTreeCascVarNegDCAPVSigmaY2",&fTreeCascVarNegDCAPVSigmaY2,"fTreeCascVarNegDCAPVSigmaY2/F");
            fTreeCascade->Branch("fTreeCascVarNegDCAPVSigmaZ2",&fTreeCascVarNegDCAPVSigmaZ2,"fTreeCascVarNegDCAPVSigmaZ2/F");
            fTreeCascade->Branch("fTreeCascVarPosDCAPVSigmaX2",&fTreeCascVarPosDCAPVSigmaX2,"fTreeCascVarPosDCAPVSigmaX2/F");
            fTreeCascade->Branch("fTreeCascVarPosDCAPVSigmaY2",&fTreeCascVarPosDCAPVSigmaY2,"fTreeCascVarPosDCAPVSigmaY2/F");
            fTreeCascade->Branch("fTreeCascVarPosDCAPVSigmaZ2",&fTreeCascVarPosDCAPVSigmaZ2,"fTreeCascVarPosDCAPVSigmaZ2/F");
            fTreeCascade->Branch("fTreeCascVarBachDCAPVSigmaX2",&fTreeCascVarBachDCAPVSigmaX2,"fTreeCascVarBachDCAPVSigmaX2/F");
            fTreeCascade->Branch("fTreeCascVarBachDCAPVSigmaY2",&fTreeCascVarBachDCAPVSigmaY2,"fTreeCascVarBachDCAPVSigmaY2/F");
            fTreeCascade->Branch("fTreeCascVarBachDCAPVSigmaZ2",&fTreeCascVarBachDCAPVSigmaZ2,"fTreeCascVarBachDCAPVSigmaZ2/F");
        }
        if ( fkDebugParenthood ){
            fTreeCascade->Branch("fTreeCascVarNegIndex",&fTreeCascVarNegIndex,"fTreeCascVarNegIndex/I");
            fTreeCascade->Branch("fTreeCascVarPosIndex",&fTreeCascVarPosIndex,"fTreeCascVarPosIndex/I");
            fTreeCascade->Branch("fTreeCascVarBachIndex",&fTreeCascVarBachIndex,"fTreeCascVarBachIndex/I");
            fTreeCascade->Branch("fTreeCascVarNegLabel",&fTreeCascVarNegLabel,"fTreeCascVarNegLabel/I");
            fTreeCascade->Branch("fTreeCascVarPosLabel",&fTreeCascVarPosLabel,"fTreeCascVarPosLabel/I");
            fTreeCascade->Branch("fTreeCascVarBachLabel",&fTreeCascVarBachLabel,"fTreeCascVarBachLabel/I");
            //Even more parenthood information
            fTreeCascade->Branch("fTreeCascVarNegLabelMother",&fTreeCascVarNegLabelMother,"fTreeCascVarNegLabelMother/I");
            fTreeCascade->Branch("fTreeCascVarPosLabelMother",&fTreeCascVarPosLabelMother,"fTreeCascVarPosLabelMother/I");
            fTreeCascade->Branch("fTreeCascVarBachLabelMother",&fTreeCascVarBachLabelMother,"fTreeCascVarBachLabelMother/I");
            fTreeCascade->Branch("fTreeCascVarNegLabelGrandMother",&fTreeCascVarNegLabelGrandMother,"fTreeCascVarNegLabelGrandMother/I");
            fTreeCascade->Branch("fTreeCascVarPosLabelGrandMother",&fTreeCascVarPosLabelGrandMother,"fTreeCascVarPosLabelGrandMother/I");
            fTreeCascade->Branch("fTreeCascVarBachLabelGrandMother",&fTreeCascVarBachLabelGrandMother,"fTreeCascVarBachLabelGrandMother/I");
            //Event Number (check same-event index mixups)
            fTreeCascade->Branch("fTreeCascVarEventNumber",&fTreeCascVarEventNumber,"fTreeCascVarEventNumber/l");
            
            fTreeCascade->Branch("fTreeCascVarIsPhysicalPrimaryNegative",&fTreeCascVarIsPhysicalPrimaryNegative,"fTreeCascVarIsPhysicalPrimaryNegative/O");
            fTreeCascade->Branch("fTreeCascVarIsPhysicalPrimaryPositive",&fTreeCascVarIsPhysicalPrimaryPositive,"fTreeCascVarIsPhysicalPrimaryPositive/O");
            fTreeCascade->Branch("fTreeCascVarIsPhysicalPrimaryBachelor",&fTreeCascVarIsPhysicalPrimaryBachelor,"fTreeCascVarIsPhysicalPrimaryBachelor/O");
            
            fTreeCascade->Branch("fTreeCascVarIsPhysicalPrimaryNegativeMother",&fTreeCascVarIsPhysicalPrimaryNegativeMother,"fTreeCascVarIsPhysicalPrimaryNegativeMother/O");
            fTreeCascade->Branch("fTreeCascVarIsPhysicalPrimaryPositiveMother",&fTreeCascVarIsPhysicalPrimaryPositiveMother,"fTreeCascVarIsPhysicalPrimaryPositiveMother/O");
            fTreeCascade->Branch("fTreeCascVarIsPhysicalPrimaryBachelorMother",&fTreeCascVarIsPhysicalPrimaryBachelorMother,"fTreeCascVarIsPhysicalPrimaryBachelorMother/O");
            
            fTreeCascade->Branch("fTreeCascVarIsPhysicalPrimaryNegativeGrandMother",&fTreeCascVarIsPhysicalPrimaryNegativeGrandMother,"fTreeCascVarIsPhysicalPrimaryNegativeGrandMother/O");
            fTreeCascade->Branch("fTreeCascVarIsPhysicalPrimaryPositiveGrandMother",&fTreeCascVarIsPhysicalPrimaryPositiveGrandMother,"fTreeCascVarIsPhysicalPrimaryPositiveGrandMother/O");
            fTreeCascade->Branch("fTreeCascVarIsPhysicalPrimaryBachelorGrandMother",&fTreeCascVarIsPhysicalPrimaryBachelorGrandMother,"fTreeCascVarIsPhysicalPrimaryBachelorGrandMother/O");
            
            fTreeCascade->Branch("fTreeCascVarPosITSClusters0",&fTreeCascVarPosITSClusters0,"fTreeCascVarPosITSClusters0/O");
            fTreeCascade->Branch("fTreeCascVarPosITSClusters1",&fTreeCascVarPosITSClusters1,"fTreeCascVarPosITSClusters1/O");
            fTreeCascade->Branch("fTreeCascVarPosITSClusters2",&fTreeCascVarPosITSClusters2,"fTreeCascVarPosITSClusters2/O");
            fTreeCascade->Branch("fTreeCascVarPosITSClusters3",&fTreeCascVarPosITSClusters3,"fTreeCascVarPosITSClusters3/O");
            fTreeCascade->Branch("fTreeCascVarPosITSClusters4",&fTreeCascVarPosITSClusters4,"fTreeCascVarPosITSClusters4/O");
            fTreeCascade->Branch("fTreeCascVarPosITSClusters5",&fTreeCascVarPosITSClusters5,"fTreeCascVarPosITSClusters5/O");
            
            fTreeCascade->Branch("fTreeCascVarNegITSClusters0",&fTreeCascVarNegITSClusters0,"fTreeCascVarNegITSClusters0/O");
            fTreeCascade->Branch("fTreeCascVarNegITSClusters1",&fTreeCascVarNegITSClusters1,"fTreeCascVarNegITSClusters1/O");
            fTreeCascade->Branch("fTreeCascVarNegITSClusters2",&fTreeCascVarNegITSClusters2,"fTreeCascVarNegITSClusters2/O");
            fTreeCascade->Branch("fTreeCascVarNegITSClusters3",&fTreeCascVarNegITSClusters3,"fTreeCascVarNegITSClusters3/O");
            fTreeCascade->Branch("fTreeCascVarNegITSClusters4",&fTreeCascVarNegITSClusters4,"fTreeCascVarNegITSClusters4/O");
            fTreeCascade->Branch("fTreeCascVarNegITSClusters5",&fTreeCascVarNegITSClusters5,"fTreeCascVarNegITSClusters5/O");
            
            fTreeCascade->Branch("fTreeCascVarBachITSClusters0",&fTreeCascVarBachITSClusters0,"fTreeCascVarBachITSClusters0/O");
            fTreeCascade->Branch("fTreeCascVarBachITSClusters1",&fTreeCascVarBachITSClusters1,"fTreeCascVarBachITSClusters1/O");
            fTreeCascade->Branch("fTreeCascVarBachITSClusters2",&fTreeCascVarBachITSClusters2,"fTreeCascVarBachITSClusters2/O");
            fTreeCascade->Branch("fTreeCascVarBachITSClusters3",&fTreeCascVarBachITSClusters3,"fTreeCascVarBachITSClusters3/O");
            fTreeCascade->Branch("fTreeCascVarBachITSClusters4",&fTreeCascVarBachITSClusters4,"fTreeCascVarBachITSClusters4/O");
            fTreeCascade->Branch("fTreeCascVarBachITSClusters5",&fTreeCascVarBachITSClusters5,"fTreeCascVarBachITSClusters5/O");
            
            fTreeCascade->Branch("fTreeCascVarPosITSSharedClusters0",&fTreeCascVarPosITSSharedClusters0,"fTreeCascVarPosITSSharedClusters0/O");
            fTreeCascade->Branch("fTreeCascVarPosITSSharedClusters1",&fTreeCascVarPosITSSharedClusters1,"fTreeCascVarPosITSSharedClusters1/O");
            fTreeCascade->Branch("fTreeCascVarPosITSSharedClusters2",&fTreeCascVarPosITSSharedClusters2,"fTreeCascVarPosITSSharedClusters2/O");
            fTreeCascade->Branch("fTreeCascVarPosITSSharedClusters3",&fTreeCascVarPosITSSharedClusters3,"fTreeCascVarPosITSSharedClusters3/O");
            fTreeCascade->Branch("fTreeCascVarPosITSSharedClusters4",&fTreeCascVarPosITSSharedClusters4,"fTreeCascVarPosITSSharedClusters4/O");
            fTreeCascade->Branch("fTreeCascVarPosITSSharedClusters5",&fTreeCascVarPosITSSharedClusters5,"fTreeCascVarPosITSSharedClusters5/O");
            
            fTreeCascade->Branch("fTreeCascVarNegITSSharedClusters0",&fTreeCascVarNegITSSharedClusters0,"fTreeCascVarNegITSSharedClusters0/O");
            fTreeCascade->Branch("fTreeCascVarNegITSSharedClusters1",&fTreeCascVarNegITSSharedClusters1,"fTreeCascVarNegITSSharedClusters1/O");
            fTreeCascade->Branch("fTreeCascVarNegITSSharedClusters2",&fTreeCascVarNegITSSharedClusters2,"fTreeCascVarNegITSSharedClusters2/O");
            fTreeCascade->Branch("fTreeCascVarNegITSSharedClusters3",&fTreeCascVarNegITSSharedClusters3,"fTreeCascVarNegITSSharedClusters3/O");
            fTreeCascade->Branch("fTreeCascVarNegITSSharedClusters4",&fTreeCascVarNegITSSharedClusters4,"fTreeCascVarNegITSSharedClusters4/O");
            fTreeCascade->Branch("fTreeCascVarNegITSSharedClusters5",&fTreeCascVarNegITSSharedClusters5,"fTreeCascVarNegITSSharedClusters5/O");
            
            fTreeCascade->Branch("fTreeCascVarBachITSSharedClusters0",&fTreeCascVarBachITSSharedClusters0,"fTreeCascVarBachITSSharedClusters0/O");
            fTreeCascade->Branch("fTreeCascVarBachITSSharedClusters1",&fTreeCascVarBachITSSharedClusters1,"fTreeCascVarBachITSSharedClusters1/O");
            fTreeCascade->Branch("fTreeCascVarBachITSSharedClusters2",&fTreeCascVarBachITSSharedClusters2,"fTreeCascVarBachITSSharedClusters2/O");
            fTreeCascade->Branch("fTreeCascVarBachITSSharedClusters3",&fTreeCascVarBachITSSharedClusters3,"fTreeCascVarBachITSSharedClusters3/O");
            fTreeCascade->Branch("fTreeCascVarBachITSSharedClusters4",&fTreeCascVarBachITSSharedClusters4,"fTreeCascVarBachITSSharedClusters4/O");
            fTreeCascade->Branch("fTreeCascVarBachITSSharedClusters5",&fTreeCascVarBachITSSharedClusters5,"fTreeCascVarBachITSSharedClusters5/O");
            
            //Uncertainty information on mass (from KF) for testing purposes
            fTreeCascade->Branch("fTreeCascVarV0LambdaMassError",&fTreeCascVarV0LambdaMassError,"fTreeCascVarV0LambdaMassError/F");
            fTreeCascade->Branch("fTreeCascVarV0AntiLambdaMassError",&fTreeCascVarV0AntiLambdaMassError,"fTreeCascVarV0AntiLambdaMassError/F");
            
            fTreeCascade->Branch("fTreeCascVarBachIsKink",&fTreeCascVarBachIsKink,"fTreeCascVarBachIsKink/O");
            fTreeCascade->Branch("fTreeCascVarPosIsKink",&fTreeCascVarPosIsKink,"fTreeCascVarPosIsKink/O");
            fTreeCascade->Branch("fTreeCascVarNegIsKink",&fTreeCascVarNegIsKink,"fTreeCascVarNegIsKink/O");
        }
        if ( fkDebugOOBPileup ) {
            fTreeCascade->Branch("fTreeCascVarNegTOFExpTDiff",&fTreeCascVarNegTOFExpTDiff,"fTreeCascVarNegTOFExpTDiff/F");
            fTreeCascade->Branch("fTreeCascVarPosTOFExpTDiff",&fTreeCascVarPosTOFExpTDiff,"fTreeCascVarPosTOFExpTDiff/F");
            fTreeCascade->Branch("fTreeCascVarBachTOFExpTDiff",&fTreeCascVarBachTOFExpTDiff,"fTreeCascVarBachTOFExpTDiff/F");
            fTreeCascade->Branch("fTreeCascVarNegTOFSignal",&fTreeCascVarNegTOFSignal,"fTreeCascVarNegTOFSignal/F");
            fTreeCascade->Branch("fTreeCascVarPosTOFSignal",&fTreeCascVarPosTOFSignal,"fTreeCascVarPosTOFSignal/F");
            fTreeCascade->Branch("fTreeCascVarBachTOFSignal",&fTreeCascVarBachTOFSignal,"fTreeCascVarBachTOFSignal/F");
            // Event info
            fTreeCascade->Branch("fTreeCascVarOOBPileupFlag",&fTreeCascVarOOBPileupFlag,"fTreeCascVarOOBPileupFlag/O");
            fTreeCascade->Branch("fTreeCascVarAmplitudeV0A",&fTreeCascVarAmplitudeV0A,"fTreeCascVarAmplitudeV0A/F");
            fTreeCascade->Branch("fTreeCascVarAmplitudeV0C",&fTreeCascVarAmplitudeV0C,"fTreeCascVarAmplitudeV0C/F");
            fTreeCascade->Branch("fTreeCascVarNHitsFMDA",&fTreeCascVarNHitsFMDA,"fTreeCascVarNHitsFMDA/F");
            fTreeCascade->Branch("fTreeCascVarNHitsFMDC",&fTreeCascVarNHitsFMDC,"fTreeCascVarNHitsFMDC/F");
        }
        
        if( fkSandboxMode ){
            //Full track info for DCA minim optimization
            fTreeCascade->Branch("fTreeCascVarBachTrack", &fTreeCascVarBachTrack,16000,99);
            fTreeCascade->Branch("fTreeCascVarPosTrack", &fTreeCascVarPosTrack,16000,99);
            fTreeCascade->Branch("fTreeCascVarNegTrack", &fTreeCascVarNegTrack,16000,99);
            
            //for sandbox mode
            fTreeCascade->Branch("fTreeCascVarMagneticField",&fTreeCascVarMagneticField,"fTreeCascVarMagneticField/F");
            
            //Cascade decay position calculation metrics
            fTreeCascade->Branch("fTreeCascVarPrimVertexX",&fTreeCascVarPrimVertexX,"fTreeCascVarPrimVertexX/F");
            fTreeCascade->Branch("fTreeCascVarPrimVertexY",&fTreeCascVarPrimVertexY,"fTreeCascVarPrimVertexY/F");
            fTreeCascade->Branch("fTreeCascVarPrimVertexZ",&fTreeCascVarPrimVertexZ,"fTreeCascVarPrimVertexZ/F");
        }
        
        //-----------MC Exclusive info--------------------
        fTreeCascade->Branch("fTreeCascVarIsPhysicalPrimary",&fTreeCascVarIsPhysicalPrimary,"fTreeCascVarIsPhysicalPrimary/I");
        fTreeCascade->Branch("fTreeCascVarPID",&fTreeCascVarPID,"fTreeCascVarPID/I");
        fTreeCascade->Branch("fTreeCascVarSwappedPID",&fTreeCascVarSwappedPID,"fTreeCascVarSwappedPID/I");
        if ( fkDebugBump ){
            fTreeCascade->Branch("fTreeCascVarPIDNegative",&fTreeCascVarPIDNegative,"fTreeCascVarPIDNegative/I");
            fTreeCascade->Branch("fTreeCascVarPIDPositive",&fTreeCascVarPIDPositive,"fTreeCascVarPIDPositive/I");
            fTreeCascade->Branch("fTreeCascVarPIDBachelor",&fTreeCascVarPIDBachelor,"fTreeCascVarPIDBachelor/I");
            fTreeCascade->Branch("fTreeCascVarPIDNegativeMother",&fTreeCascVarPIDNegativeMother,"fTreeCascVarPIDNegativeMother/I");
            fTreeCascade->Branch("fTreeCascVarPIDPositiveMother",&fTreeCascVarPIDPositiveMother,"fTreeCascVarPIDPositiveMother/I");
            fTreeCascade->Branch("fTreeCascVarPIDBachelorMother",&fTreeCascVarPIDBachelorMother,"fTreeCascVarPIDBachelorMother/I");
            //All information possibly needed on parenthood
            fTreeCascade->Branch("fTreeCascVarPIDNegativeGrandMother",&fTreeCascVarPIDNegativeGrandMother,"fTreeCascVarPIDNegativeGrandMother/I");
            fTreeCascade->Branch("fTreeCascVarPIDPositiveGrandMother",&fTreeCascVarPIDPositiveGrandMother,"fTreeCascVarPIDPositiveGrandMother/I");
            fTreeCascade->Branch("fTreeCascVarPIDBachelorGrandMother",&fTreeCascVarPIDBachelorGrandMother,"fTreeCascVarPIDBachelorGrandMother/I");
            fTreeCascade->Branch("fTreeCascVarBachCousinStatus",&fTreeCascVarBachCousinStatus,"fTreeCascVarBachCousinStatus/I");
            fTreeCascade->Branch("fTreeCascVarV0BachSibIsValid",&fTreeCascVarV0BachSibIsValid,"fTreeCascVarV0BachSibIsValid/I");
            fTreeCascade->Branch("fTreeCascVarBachV0Tagging",&fTreeCascVarBachV0Tagging,"fTreeCascVarBachV0Tagging/I");
            fTreeCascade->Branch("fTreeCascVarV0NegSibIsValid",&fTreeCascVarV0NegSibIsValid,"fTreeCascVarV0NegSibIsValid/I");
            fTreeCascade->Branch("fTreeCascVarNegV0Tagging",&fTreeCascVarNegV0Tagging,"fTreeCascVarNegV0Tagging/I");
            fTreeCascade->Branch("fTreeCascVarV0PosSibIsValid",&fTreeCascVarV0PosSibIsValid,"fTreeCascVarV0PosSibIsValid/I");
            fTreeCascade->Branch("fTreeCascVarPosV0Tagging",&fTreeCascVarPosV0Tagging,"fTreeCascVarPosV0Tagging/I");
        }
        //------------------------------------------------
    }
    //------------------------------------------------
    // Particle Identification Setup
    //------------------------------------------------
    
    AliAnalysisManager *man=AliAnalysisManager::GetAnalysisManager();
    AliInputEventHandler* inputHandler = (AliInputEventHandler*) (man->GetInputEventHandler());
    fPIDResponse = inputHandler->GetPIDResponse();
    inputHandler->SetNeedField();
    
    // Multiplicity
    if(! fESDtrackCuts ) {
        fESDtrackCuts = AliESDtrackCuts::GetStandardITSTPCTrackCuts2010(kTRUE,kFALSE);
        fESDtrackCuts->SetPtRange(0.15);  // adding pt cut
        fESDtrackCuts->SetEtaRange(-1.0, 1.0);
    }
    //Analysis Utils
    if(! fUtils ) {
        fUtils = new AliAnalysisUtils();
    }
    if(! fRand ){
        fRand = new TRandom3();
        // From TRandom3 reference:
        // if seed is 0 (default value) a TUUID is generated and
        // used to fill the first 8 integers of the seed array.
        fRand->SetSeed(0);
    }
    
    // OOB Pileup in pp 2016
    if( !fESDtrackCutsGlobal2015 && fkDebugOOBPileup ) {
        fESDtrackCutsGlobal2015 = AliESDtrackCuts::GetStandardITSTPCTrackCuts2015PbPb(kTRUE,kFALSE);
        //Initial set of cuts - to be adjusted
        fESDtrackCutsGlobal2015->SetPtRange(0.15);
        fESDtrackCutsGlobal2015->SetEtaRange(-1.0, 1.0);
    }
    if( !fESDtrackCutsITSsa2010 && fkDebugOOBPileup ) {
        fESDtrackCutsITSsa2010 = AliESDtrackCuts::GetStandardITSSATrackCuts2010();
    }
    
    //------------------------------------------------
    // V0 Multiplicity Histograms
    //------------------------------------------------
    
    // Create histograms
    fListHist = new TList();
    fListHist->SetOwner();  // See http://root.cern.ch/root/html/TCollection.html#TCollection:SetOwner
    
    fEventCuts.AddQAplotsToList(fListHist);
    
    if(! fHistEventCounter ) {
        //Histogram Output: Event-by-Event
        fHistEventCounter = new TH1D( "fHistEventCounter", ";Evt. Sel. Step;Count",2,0,2);
        fHistEventCounter->GetXaxis()->SetBinLabel(1, "Processed");
        fHistEventCounter->GetXaxis()->SetBinLabel(2, "Selected");
        fListHist->Add(fHistEventCounter);
    }
    
    if(! fHistCentrality ) {
        //Histogram Output: Event-by-Event
        fHistCentrality = new TH1D( "fHistCentrality", "WARNING: no pileup rejection applied!;Centrality;Event Count",100,0,100);
        fListHist->Add(fHistCentrality);
    }
    
    if(! fHistGeneratedPtVsYVsCentralityK0Short ) {
        //Histogram Output: Efficiency Denominator
        fHistGeneratedPtVsYVsCentralityK0Short = new TH3D( "fHistGeneratedPtVsYVsCentralityK0Short", ";pT;y;centrality",500,0,25,40,-1.0,1.0,100,0,100);
        fListHist->Add(fHistGeneratedPtVsYVsCentralityK0Short);
    }
    if(! fHistGeneratedPtVsYVsCentralityLambda ) {
        //Histogram Output: Efficiency Denominator
        fHistGeneratedPtVsYVsCentralityLambda = new TH3D( "fHistGeneratedPtVsYVsCentralityLambda", ";pT;y;centrality",500,0,25,40,-1.0,1.0,100,0,100);
        fListHist->Add(fHistGeneratedPtVsYVsCentralityLambda);
    }
    if(! fHistGeneratedPtVsYVsCentralityAntiLambda ) {
        //Histogram Output: Efficiency Denominator
        fHistGeneratedPtVsYVsCentralityAntiLambda = new TH3D( "fHistGeneratedPtVsYVsCentralityAntiLambda", ";pT;y;centrality",500,0,25,40,-1.0,1.0,100,0,100);
        fListHist->Add(fHistGeneratedPtVsYVsCentralityAntiLambda);
    }
    if(! fHistGeneratedPtVsYVsCentralityXiMinus ) {
        //Histogram Output: Efficiency Denominator
        fHistGeneratedPtVsYVsCentralityXiMinus = new TH3D( "fHistGeneratedPtVsYVsCentralityXiMinus", ";pT;y;centrality",500,0,25,40,-1.0,1.0,100,0,100);
        fListHist->Add(fHistGeneratedPtVsYVsCentralityXiMinus);
    }
    if(! fHistGeneratedPtVsYVsCentralityXiPlus ) {
        //Histogram Output: Efficiency Denominator
        fHistGeneratedPtVsYVsCentralityXiPlus = new TH3D( "fHistGeneratedPtVsYVsCentralityXiPlus", ";pT;y;centrality",500,0,25,40,-1.0,1.0,100,0,100);
        fListHist->Add(fHistGeneratedPtVsYVsCentralityXiPlus);
    }
    if(! fHistGeneratedPtVsYVsCentralityOmegaMinus ) {
        //Histogram Output: Efficiency Denominator
        fHistGeneratedPtVsYVsCentralityOmegaMinus = new TH3D( "fHistGeneratedPtVsYVsCentralityOmegaMinus", ";pT;y;centrality",500,0,25,40,-1.0,1.0,100,0,100);
        fListHist->Add(fHistGeneratedPtVsYVsCentralityOmegaMinus);
    }
    if(! fHistGeneratedPtVsYVsCentralityOmegaPlus ) {
        //Histogram Output: Efficiency Denominator
        fHistGeneratedPtVsYVsCentralityOmegaPlus = new TH3D( "fHistGeneratedPtVsYVsCentralityOmegaPlus", ";pT;y;centrality",500,0,25,40,-1.0,1.0,100,0,100);
        fListHist->Add(fHistGeneratedPtVsYVsCentralityOmegaPlus);
    }
    
    //Superlight mode output
    if ( !fListV0 ){
        fListV0 = new TList();
        fListV0->SetOwner();
    }
    
    if ( !fListCascade ){
        //Superlight mode output
        fListCascade = new TList();
        fListCascade->SetOwner();
    }
    
    //Regular Output: Slots 1, 2, 3
    PostData(1, fListHist    );
    PostData(2, fListV0      );
    PostData(3, fListCascade );
    
    //TTree Objects: Slots 4, 5, 6
    if(fkSaveEventTree)    PostData(4, fTreeEvent   );
    if(fkSaveV0Tree)       PostData(5, fTreeV0      );
    if(fkSaveCascadeTree)  PostData(6, fTreeCascade );
    
}// end UserCreateOutputObjects


//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityMCRun2::UserExec(Option_t *)
{
    // Main loop
    // Called for each event
    AliESDEvent *lESDevent = 0x0;
    AliMCEvent  *lMCevent  = 0x0;
    AliStack    *lMCstack  = 0x0;
    
    // Connect to the InputEvent
    // After these lines, we should have an ESD/AOD event + the number of V0s in it.
    
    // Appropriate for ESD analysis!
    
    lESDevent = dynamic_cast<AliESDEvent*>( InputEvent() );
    if (!lESDevent) {
        AliWarning("ERROR: lESDevent not available \n");
        return;
    }
    
    //=================================================
    // Monte Carlo-related information
    lMCevent = MCEvent();
    if (!lMCevent) {
        Printf("ERROR: Could not retrieve MC event \n");
        cout << "Name of the file with pb :" <<  fInputHandler->GetTree()->GetCurrentFile()->GetName() << endl;
        return;
    }
    lMCstack = lMCevent->Stack();
    if (!lMCstack) {
        Printf("ERROR: Could not retrieve MC stack \n");
        cout << "Name of the file with pb :" <<  fInputHandler->GetTree()->GetCurrentFile()->GetName() << endl;
        return;
    }
    //=================================================
    
    //Get VZERO Information for multiplicity later
    AliVVZERO* esdV0 = lESDevent->GetVZEROData();
    if (!esdV0) {
        AliError("AliVVZERO not available");
        return;
    }
    
    Double_t lMagneticField = -10;
    lMagneticField = lESDevent->GetMagneticField( );
    fTreeCascVarMagField = lMagneticField;
    
    //------------------------------------------------
    // Event Selection ---
    //  --- Performed entirely via AliPPVsMultUtils
    // (except removal of incomplete events and SPDClusterVsTracklets cut)
    //------------------------------------------------
    
    //Copy-paste of steps done in AliAnalysisTaskSkeleton
    
    fHistEventCounter->Fill(0.5);
    
    //------------------------------------------------
    // Primary Vertex Requirements Section:
    //  ---> pp: has vertex, |z|<10cm
    //------------------------------------------------
    
    //classical Proton-proton like selection
    const AliESDVertex *lPrimaryBestESDVtx     = lESDevent->GetPrimaryVertex();
    const AliESDVertex *lPrimaryTrackingESDVtx = lESDevent->GetPrimaryVertexTracks();
    const AliESDVertex *lPrimarySPDVtx         = lESDevent->GetPrimaryVertexSPD();
    
    Double_t lBestPrimaryVtxPos[3]          = {-100.0, -100.0, -100.0};
    lPrimaryBestESDVtx->GetXYZ( lBestPrimaryVtxPos );
    
    //------------------------------------------------
    // Multiplicity Information Acquistion
    //------------------------------------------------
    
    Float_t lPercentile = 500;
    Float_t lPercentileEmbeddedSelection = 500;
    Int_t lEvSelCode = 100;
    AliMultSelection *MultSelection = (AliMultSelection*) lESDevent -> FindListObject("MultSelection");
    if( !MultSelection) {
        //If you get this warning (and lPercentiles 300) please check that the AliMultSelectionTask actually ran (before your task)
        AliWarning("AliMultSelection object not found!");
    } else {
        //V0M Multiplicity Percentile
        lPercentile = MultSelection->GetMultiplicityPercentile("V0M");
        lPercentileEmbeddedSelection = MultSelection->GetMultiplicityPercentile("V0M", kTRUE );
        //Event Selection Code
        lEvSelCode = MultSelection->GetEvSelCode();
    }
    
    //just ask AliMultSelection. It will know.
    fMVPileupFlag = kFALSE;
    fMVPileupFlag = MultSelection->GetThisEventIsNotPileupMV();
    
    fCentrality = lPercentile;
    
    if( lEvSelCode != 0 ) {
        PostData(1, fListHist    );
        PostData(2, fListV0      );
        PostData(3, fListCascade );
        if( fkSaveEventTree   ) PostData(4, fTreeEvent   );
        if( fkSaveV0Tree      ) PostData(5, fTreeV0      );
        if( fkSaveCascadeTree ) PostData(6, fTreeCascade );
        return;
    }
    
    AliVEvent *ev = InputEvent();
    if( fkDoExtraEvSels ) {
        if( !fEventCuts.AcceptEvent(ev) ) {
            PostData(1, fListHist    );
            PostData(2, fListV0      );
            PostData(3, fListCascade );
            if( fkSaveEventTree   ) PostData(4, fTreeEvent   );
            if( fkSaveV0Tree      ) PostData(5, fTreeV0      );
            if( fkSaveCascadeTree ) PostData(6, fTreeCascade );
            return;
        }
    }
    
    fHistEventCounter->Fill(1.5);
    
    //Bookkeep event number for debugging
    fTreeCascVarEventNumber =
    ( ( ((ULong64_t)lESDevent->GetPeriodNumber() ) << 36 ) |
     ( ((ULong64_t)lESDevent->GetOrbitNumber () ) << 12 ) |
     ((ULong64_t)lESDevent->GetBunchCrossNumber() )  );
    
    //Save info for pileup study (high multiplicity triggers pp 13 TeV - 2016 data)
    if( fkDebugOOBPileup ) {
        fOOBPileupFlag     = !fUtils->IsOutOfBunchPileUp(ev);
        fNTOFClusters      = lESDevent->GetESDTOFClusters()->GetEntriesFast();
        fNTOFMatches       = lESDevent->GetESDTOFMatches()->GetEntriesFast();
        fNTracksITSsa2010  = 0;
        fNTracksGlobal2015 = 0;
        fNTracksGlobal2015TriggerPP = 0;
        //Count tracks with various selections
        for(Long_t itrack = 0; itrack<lESDevent->GetNumberOfTracks(); itrack++) {
            AliVTrack *track = lESDevent -> GetVTrack( itrack );
            if( !track ) continue;
            //Only ITSsa tracks
            if( fESDtrackCutsITSsa2010->AcceptVTrack(track) ) fNTracksITSsa2010++;
            if( !fESDtrackCutsGlobal2015->AcceptVTrack(track) ) continue;
            //Only for accepted tracks
            fNTracksGlobal2015++;
            //Count accepted + TOF time window (info from Alberica)
            //Warning: 12.5 is appropriate for pp (for Pb-Pb use 30)
            if( TMath::Abs( track->GetTOFExpTDiff() ) < 12.5 ) fNTracksGlobal2015TriggerPP++;
        }
        
        //VZERO info
        fAmplitudeV0A = ((AliMultEstimator*)MultSelection->GetEstimator("V0A"))->GetValue();
        fAmplitudeV0C = ((AliMultEstimator*)MultSelection->GetEstimator("V0C"))->GetValue();
        
        //FMD info
        AliAODEvent* aodEvent = AliForwardUtil::GetAODEvent(this);
        if (!aodEvent) return;
        FMDhits fmdhits = GetFMDhits(aodEvent);
        fNHitsFMDA = std::accumulate(fmdhits.begin(), fmdhits.end(), 0,
                                     [](Float_t a, AliAnalysisTaskStrangenessVsMultiplicityMCRun2::FMDhit t) {
                                         return a + ((2.8 < t.eta && t.eta < 5.03) ? t.weight : 0.0f);
                                     });
        fNHitsFMDC = std::accumulate(fmdhits.begin(), fmdhits.end(), 0,
                                     [](Float_t a, AliAnalysisTaskStrangenessVsMultiplicityMCRun2::FMDhit t) {
                                         return a + ((-3.4 < t.eta && t.eta < 2.01) ? t.weight : 0.0f);
                                     });
        
    }
    
    
    
    //Fill centrality histogram
    fHistCentrality->Fill(fCentrality);
    
    //Event-level fill
    if ( fkSaveEventTree ) fTreeEvent->Fill() ;
    
    //STOP HERE if skipping event selections (no point in doing the rest...)
    
    //------------------------------------------------
    
    //----- Loop on Generated Particles --------------
    Int_t    lThisPDG  = 0;
    Double_t lThisRap  = 0;
    Double_t lThisPt   = 0;
    for (Int_t ilab = 0;  ilab < (lMCstack->GetNtrack()); ilab++)
    {   // This is the begining of the loop on tracks
        
        TParticle* lPart = 0x0;
        lPart = lMCstack->Particle( ilab );
        if(!lPart) {
            Printf("Generated loop %d - MC TParticle pointer to current stack particle = 0x0 ! Skip ...\n", ilab );
            continue;
        }
        
        lThisPDG = lPart->GetPdgCode();
        
        //This if is necessary in some situations (rapidity calculation and PYTHIA junctions, etc)
        if ( (TMath::Abs(lThisPDG) == 3312) || (TMath::Abs(lThisPDG) == 3334) || (TMath::Abs(lThisPDG) == 3122) || lThisPDG == 310 )
        {
            lThisRap   = MyRapidity(lPart->Energy(),lPart->Pz());
            lThisPt    = lPart->Pt();
            
            //Use Physical Primaries only for filling These Histos
            if ( lMCstack->IsPhysicalPrimary(ilab)!=kTRUE ) continue;
            
            if( lThisPDG ==   310 ) {
                fHistGeneratedPtVsYVsCentralityK0Short       -> Fill (lThisPt, lThisRap, lPercentileEmbeddedSelection);
            }
            if( lThisPDG ==  3122 ) {
                fHistGeneratedPtVsYVsCentralityLambda       -> Fill (lThisPt, lThisRap, lPercentileEmbeddedSelection);
            }
            if( lThisPDG == -3122 ) {
                fHistGeneratedPtVsYVsCentralityAntiLambda       -> Fill (lThisPt, lThisRap, lPercentileEmbeddedSelection);
            }
            if( lThisPDG ==  3312 ) {
                fHistGeneratedPtVsYVsCentralityXiMinus       -> Fill (lThisPt, lThisRap, lPercentileEmbeddedSelection);
            }
            if( lThisPDG == -3312 ) {
                fHistGeneratedPtVsYVsCentralityXiPlus       -> Fill (lThisPt, lThisRap, lPercentileEmbeddedSelection);
            }
            if( lThisPDG ==  3334 ) {
                fHistGeneratedPtVsYVsCentralityOmegaMinus       -> Fill (lThisPt, lThisRap, lPercentileEmbeddedSelection);
            }
            if( lThisPDG == -3334 ) {
                fHistGeneratedPtVsYVsCentralityOmegaPlus       -> Fill (lThisPt, lThisRap, lPercentileEmbeddedSelection);
            }
        }
    }//End of loop on tracks
    //----- End Loop on Cascades ------------------------------------------------------------
    
    //------------------------------------------------
    // Fill V0 Tree as needed
    //------------------------------------------------
    
    //Variable definition
    Int_t    lOnFlyStatus = 0;// nv0sOn = 0, nv0sOff = 0;
    Double_t lChi2V0 = 0;
    Double_t lDcaV0Daughters = 0, lDcaV0ToPrimVertex = 0;
    Double_t lDcaPosToPrimVertex = 0, lDcaNegToPrimVertex = 0;
    Double_t lV0CosineOfPointingAngle = 0;
    Double_t lV0Radius = 0, lPt = 0;
    Double_t lRapK0Short = 0, lRapLambda = 0;
    Double_t lInvMassK0s = 0, lInvMassLambda = 0, lInvMassAntiLambda = 0;
    Double_t lAlphaV0 = 0, lPtArmV0 = 0;
    
    Double_t fMinV0Pt = 0;
    Double_t fMaxV0Pt = 100;
    
    //------------------------------------------------
    // Rerun V0 Vertexer!
    // WARNING: this will only work if the
    // special "use on the fly cascading" flag
    // is disabled!
    //------------------------------------------------
    
    if( fkRunVertexers && !fkUseOnTheFlyV0Cascading ) {
        //Only reset if not using on-the-fly (or else nothing passes)
        lESDevent->ResetV0s();
        
        //Decide between regular and light vertexer (default: light)
        if ( ! fkUseLightVertexer ){
            //Instantiate vertexer object
            AliV0vertexer lV0vtxer;
            //Set Cuts
            lV0vtxer.SetDefaultCuts(fV0VertexerSels);
            lV0vtxer.SetCuts(fV0VertexerSels);
            //Redo
            lV0vtxer.Tracks2V0vertices(lESDevent);
        } else {
            //Instantiate vertexer object
            AliLightV0vertexer lV0vtxer;
            //Set do or don't do V0 refit for improved precision
            lV0vtxer.SetDoRefit( kFALSE );
            if (fkDoV0Refit) lV0vtxer.SetDoRefit(kTRUE);
            //Set Cuts
            lV0vtxer.SetDefaultCuts(fV0VertexerSels);
            lV0vtxer.SetCuts(fV0VertexerSels);
            //Redo
            lV0vtxer.Tracks2V0vertices(lESDevent);
        }
    }
    
    Int_t nv0s = 0;
    nv0s = lESDevent->GetNumberOfV0s();
    
    for (Int_t iV0 = 0; iV0 < nv0s; iV0++) //extra-crazy test
    {   // This is the begining of the V0 loop
        AliESDv0 *v0 = ((AliESDEvent*)lESDevent)->GetV0(iV0);
        if (!v0) continue;
        
        CheckChargeV0( v0 );
        //Remove like-sign (will not affect offline V0 candidates!)
        if( v0->GetParamN()->Charge() > 0 && v0->GetParamP()->Charge() > 0 ){
            continue;
        }
        if( v0->GetParamN()->Charge() < 0 && v0->GetParamP()->Charge() < 0 ){
            continue;
        }
        
        Double_t tDecayVertexV0[3];
        v0->GetXYZ(tDecayVertexV0[0],tDecayVertexV0[1],tDecayVertexV0[2]);
        
        Double_t tV0mom[3];
        v0->GetPxPyPz( tV0mom[0],tV0mom[1],tV0mom[2] );
        Double_t lV0TotalMomentum = TMath::Sqrt(
                                                tV0mom[0]*tV0mom[0]+tV0mom[1]*tV0mom[1]+tV0mom[2]*tV0mom[2] );
        
        lV0Radius = TMath::Sqrt(tDecayVertexV0[0]*tDecayVertexV0[0]+tDecayVertexV0[1]*tDecayVertexV0[1]);
        
        lPt = v0->Pt();
        lRapK0Short = v0->RapK0Short();
        lRapLambda  = v0->RapLambda();
        if ((lPt<fMinV0Pt)||(fMaxV0Pt<lPt)) continue;
        
        UInt_t lKeyPos = (UInt_t)TMath::Abs(v0->GetPindex());
        UInt_t lKeyNeg = (UInt_t)TMath::Abs(v0->GetNindex());
        
        Double_t lMomPos[3];
        v0->GetPPxPyPz(lMomPos[0],lMomPos[1],lMomPos[2]);
        Double_t lMomNeg[3];
        v0->GetNPxPyPz(lMomNeg[0],lMomNeg[1],lMomNeg[2]);
        
        AliESDtrack *pTrack=((AliESDEvent*)lESDevent)->GetTrack(lKeyPos);
        AliESDtrack *nTrack=((AliESDEvent*)lESDevent)->GetTrack(lKeyNeg);
        if (!pTrack || !nTrack) {
            Printf("ERROR: Could not retreive one of the daughter track");
            continue;
        }
        fTreeVariablePosPIDForTracking = pTrack->GetPIDForTracking();
        fTreeVariableNegPIDForTracking = nTrack->GetPIDForTracking();
        
        const AliExternalTrackParam *innernegv0=nTrack->GetInnerParam();
        const AliExternalTrackParam *innerposv0=pTrack->GetInnerParam();
        Float_t lThisPosInnerP = -1;
        Float_t lThisNegInnerP = -1;
        Float_t lThisPosInnerPt = -1;
        Float_t lThisNegInnerPt = -1;
        if(innerposv0)  { lThisPosInnerP  = innerposv0 ->GetP(); }
        if(innernegv0)  { lThisNegInnerP  = innernegv0 ->GetP(); }
        if(innerposv0)  { lThisPosInnerPt  = innerposv0 ->Pt(); }
        if(innernegv0)  { lThisNegInnerPt  = innernegv0 ->Pt(); }
        Float_t lThisPosdEdx = pTrack -> GetTPCsignal();
        Float_t lThisNegdEdx = nTrack -> GetTPCsignal();
        
        fTreeVariablePosdEdx = lThisPosdEdx;
        fTreeVariableNegdEdx = lThisNegdEdx;
        
        fTreeVariablePosInnerP = lThisPosInnerP;
        fTreeVariableNegInnerP = lThisNegInnerP;
        
        //Daughter Eta for Eta selection, afterwards
        fTreeVariableNegEta = nTrack->Eta();
        fTreeVariablePosEta = pTrack->Eta();
        
        if ( fkExtraCleanup ){
            if( TMath::Abs(fTreeVariableNegEta)>0.8 || TMath::Abs(fTreeVariableNegEta)>0.8 ) continue;
            if( TMath::Abs(lRapK0Short        )>0.5 && TMath::Abs(lRapLambda         )>0.5 ) continue;
        }
        
        // Filter like-sign V0 (next: add counter and distribution)
        if ( pTrack->GetSign() == nTrack->GetSign()) {
            continue;
        }
        
        //________________________________________________________________________
        // Track quality cuts
        Float_t lPosTrackCrossedRows = pTrack->GetTPCClusterInfo(2,1);
        Float_t lNegTrackCrossedRows = nTrack->GetTPCClusterInfo(2,1);
        fTreeVariableLeastNbrCrossedRows = (Int_t) lPosTrackCrossedRows;
        if( lNegTrackCrossedRows < fTreeVariableLeastNbrCrossedRows )
            fTreeVariableLeastNbrCrossedRows = (Int_t) lNegTrackCrossedRows;
        
        // TPC refit condition (done during reconstruction for Offline but not for On-the-fly)
        if( !(pTrack->GetStatus() & AliESDtrack::kTPCrefit)) continue;
        if( !(nTrack->GetStatus() & AliESDtrack::kTPCrefit)) continue;
        
        //Get status flags
        fTreeVariablePosTrackStatus = pTrack->GetStatus();
        fTreeVariableNegTrackStatus = nTrack->GetStatus();
        
        fTreeVariablePosDCAz = GetDCAz(pTrack);
        fTreeVariableNegDCAz = GetDCAz(nTrack);
        
        //GetKinkIndex condition
        if( pTrack->GetKinkIndex(0)>0 || nTrack->GetKinkIndex(0)>0 ) continue;
        
        //Findable clusters > 0 condition
        if( pTrack->GetTPCNclsF()<=0 || nTrack->GetTPCNclsF()<=0 ) continue;
        
        //Compute ratio Crossed Rows / Findable clusters
        //Note: above test avoids division by zero!
        Float_t lPosTrackCrossedRowsOverFindable = lPosTrackCrossedRows / ((double)(pTrack->GetTPCNclsF()));
        Float_t lNegTrackCrossedRowsOverFindable = lNegTrackCrossedRows / ((double)(nTrack->GetTPCNclsF()));
        
        fTreeVariableLeastRatioCrossedRowsOverFindable = lPosTrackCrossedRowsOverFindable;
        if( lNegTrackCrossedRowsOverFindable < fTreeVariableLeastRatioCrossedRowsOverFindable )
            fTreeVariableLeastRatioCrossedRowsOverFindable = lNegTrackCrossedRowsOverFindable;
        
        //Lowest Cut Level for Ratio Crossed Rows / Findable = 0.8, set here
        //if ( fTreeVariableLeastRatioCrossedRowsOverFindable < 0.8 ) continue;
        
        //Extra track quality: Chi2/cluster for cross-checks
        Float_t lBiggestChi2PerCluster = -1;
        
        Float_t lPosChi2PerCluster = 1000;
        Float_t lNegChi2PerCluster = 1000;
        
        if( pTrack->GetTPCNcls() > 0 ) lPosChi2PerCluster = pTrack->GetTPCchi2() / ((Float_t)pTrack->GetTPCNcls());
        if( nTrack->GetTPCNcls() > 0 ) lNegChi2PerCluster = nTrack->GetTPCchi2() / ((Float_t)nTrack->GetTPCNcls());
        
        if ( lPosChi2PerCluster  > lBiggestChi2PerCluster ) lBiggestChi2PerCluster = lPosChi2PerCluster;
        if ( lNegChi2PerCluster  > lBiggestChi2PerCluster ) lBiggestChi2PerCluster = lNegChi2PerCluster;
        
        fTreeVariableMaxChi2PerCluster = lBiggestChi2PerCluster;
        
        //Extra track quality: min track length
        Float_t lSmallestTrackLength = 1000;
        Float_t lPosTrackLength = -1;
        Float_t lNegTrackLength = -1;
        
        if (pTrack->GetInnerParam()) lPosTrackLength = pTrack->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
        if (nTrack->GetInnerParam()) lNegTrackLength = nTrack->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
        
        if ( lPosTrackLength  < lSmallestTrackLength ) lSmallestTrackLength = lPosTrackLength;
        if ( lNegTrackLength  < lSmallestTrackLength ) lSmallestTrackLength = lNegTrackLength;
        
        fTreeVariableMinTrackLength = lSmallestTrackLength;
        
        if ( ( ( ( pTrack->GetTPCClusterInfo(2,1) ) < 70 ) || ( ( nTrack->GetTPCClusterInfo(2,1) ) < 70 ) ) && lSmallestTrackLength<80 ) continue;
        
        //End track Quality Cuts
        //________________________________________________________________________
        
        lDcaPosToPrimVertex = TMath::Abs(pTrack->GetD(lBestPrimaryVtxPos[0],
                                                      lBestPrimaryVtxPos[1],
                                                      lMagneticField) );
        
        lDcaNegToPrimVertex = TMath::Abs(nTrack->GetD(lBestPrimaryVtxPos[0],
                                                      lBestPrimaryVtxPos[1],
                                                      lMagneticField) );
        
        lOnFlyStatus = v0->GetOnFlyStatus();
        lChi2V0 = v0->GetChi2V0();
        lDcaV0Daughters = v0->GetDcaV0Daughters();
        lDcaV0ToPrimVertex = v0->GetD(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2]);
        lV0CosineOfPointingAngle = v0->GetV0CosineOfPointingAngle(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2]);
        fTreeVariableV0CosineOfPointingAngle=lV0CosineOfPointingAngle;
        
        // Getting invariant mass infos directly from ESD
        v0->ChangeMassHypothesis(310);
        lInvMassK0s = v0->GetEffMass();
        v0->ChangeMassHypothesis(3122);
        lInvMassLambda = v0->GetEffMass();
        v0->ChangeMassHypothesis(-3122);
        lInvMassAntiLambda = v0->GetEffMass();
        lAlphaV0 = v0->AlphaV0();
        lPtArmV0 = v0->PtArmV0();
        
        fTreeVariableMVPileupFlag = fMVPileupFlag;
        
        fTreeVariablePt = v0->Pt();
        fTreeVariableChi2V0 = lChi2V0;
        fTreeVariableDcaV0ToPrimVertex = lDcaV0ToPrimVertex;
        fTreeVariableDcaV0Daughters = lDcaV0Daughters;
        fTreeVariableV0CosineOfPointingAngle = lV0CosineOfPointingAngle;
        fTreeVariableV0Radius = lV0Radius;
        fTreeVariableDcaPosToPrimVertex = lDcaPosToPrimVertex;
        fTreeVariableDcaNegToPrimVertex = lDcaNegToPrimVertex;
        fTreeVariableInvMassK0s = lInvMassK0s;
        fTreeVariableInvMassLambda = lInvMassLambda;
        fTreeVariableInvMassAntiLambda = lInvMassAntiLambda;
        fTreeVariableRapK0Short = lRapK0Short;
        fTreeVariableRapLambda = lRapLambda;
        fTreeVariableAlphaV0 = lAlphaV0;
        fTreeVariablePtArmV0 = lPtArmV0;
        
        //Official means of acquiring N-sigmas
        fTreeVariableNSigmasPosProton = fPIDResponse->NumberOfSigmasTPC( pTrack, AliPID::kProton );
        fTreeVariableNSigmasPosPion   = fPIDResponse->NumberOfSigmasTPC( pTrack, AliPID::kPion );
        fTreeVariableNSigmasNegProton = fPIDResponse->NumberOfSigmasTPC( nTrack, AliPID::kProton );
        fTreeVariableNSigmasNegPion   = fPIDResponse->NumberOfSigmasTPC( nTrack, AliPID::kPion );
        
        //This requires an Invariant Mass Hypothesis afterwards
        fTreeVariableDistOverTotMom = TMath::Sqrt(
                                                  TMath::Power( tDecayVertexV0[0] - lBestPrimaryVtxPos[0] , 2) +
                                                  TMath::Power( tDecayVertexV0[1] - lBestPrimaryVtxPos[1] , 2) +
                                                  TMath::Power( tDecayVertexV0[2] - lBestPrimaryVtxPos[2] , 2)
                                                  );
        fTreeVariableDistOverTotMom /= (lV0TotalMomentum+1e-10); //avoid division by zero, to be sure
        
        //Copy Multiplicity information
        fTreeVariableCentrality = fCentrality;
        
        //Info for pileup studies
        if( fkDebugOOBPileup ) {
            fTreeVariableNegTOFExpTDiff = nTrack->GetTOFExpTDiff( lESDevent->GetMagneticField() );
            fTreeVariablePosTOFExpTDiff = pTrack->GetTOFExpTDiff( lESDevent->GetMagneticField() );
            fTreeVariableNegTOFSignal = nTrack->GetTOFsignal() * 1.e-3; // in ns
            fTreeVariablePosTOFSignal = pTrack->GetTOFsignal() * 1.e-3; // in ns
            //Copy OOB pileup flag for this event
            fTreeVariableOOBPileupFlag = fOOBPileupFlag;
            //Copy VZERO information for this event
            fTreeVariableAmplitudeV0A = fAmplitudeV0A;
            fTreeVariableAmplitudeV0C = fAmplitudeV0C;
            //Copy FMD information for this event
            fTreeVariableNHitsFMDA = fNHitsFMDA;
            fTreeVariableNHitsFMDC = fNHitsFMDC;
        }
        
        //===============================================
        // V0 Monte Carlo Association starts here
        //===============================================
        
        //---> Set Everything to "I don't know" before starting
        //---> This is strictly necessary!
        
        fTreeVariablePIDPositive = 0;
        fTreeVariablePIDNegative = 0;
        
        fTreeVariablePtMother = -1;
        fTreeVariableRapMother = -100;
        fTreeVariablePtMC = -1;
        fTreeVariableRapMC = -100;
        
        fTreeVariablePID = -1;
        fTreeVariablePIDMother = -1;
        
        fTreeVariablePrimaryStatus = 0;
        fTreeVariablePrimaryStatusMother = 0;
        
        //fTreeVariablePosTransvMomentumMC = -1;
        //fTreeVariableNegTransvMomentumMC = -1;
        
        Int_t lblPosV0Dghter = (Int_t) TMath::Abs( pTrack->GetLabel() );
        Int_t lblNegV0Dghter = (Int_t) TMath::Abs( nTrack->GetLabel() );
        
        TParticle* mcPosV0Dghter = lMCstack->Particle( lblPosV0Dghter );
        TParticle* mcNegV0Dghter = lMCstack->Particle( lblNegV0Dghter );
        
        Int_t lPIDPositive = mcPosV0Dghter -> GetPdgCode();
        Int_t lPIDNegative = mcNegV0Dghter -> GetPdgCode();
        
        //fTreeVariablePosTransvMomentumMC = mcPosV0Dghter->Pt();
        //fTreeVariableNegTransvMomentumMC = mcNegV0Dghter->Pt();
        
        fTreeVariablePIDPositive = lPIDPositive;
        fTreeVariablePIDNegative = lPIDNegative;
        
        Int_t lblMotherPosV0Dghter = mcPosV0Dghter->GetFirstMother();
        Int_t lblMotherNegV0Dghter = mcNegV0Dghter->GetFirstMother();
        
        Double_t lMCTransvMomNeg = mcNegV0Dghter->Pt();
        Double_t lMCTransvMomPos = mcPosV0Dghter->Pt();
        
        if( lblMotherPosV0Dghter == lblMotherNegV0Dghter && lblMotherPosV0Dghter > -1 ) {
            //either label is fine, they're equal at this stage
            TParticle* pThisV0 = lMCstack->Particle( lblMotherPosV0Dghter );
            //Set tree variables
            fTreeVariablePID   = pThisV0->GetPdgCode(); //PDG Code
            fTreeVariablePtMC  = pThisV0->Pt(); //Perfect Pt
            
            //Only Interested if it's a Lambda, AntiLambda or K0s
            //Avoid the Junction Bug! PYTHIA has particles with Px=Py=Pz=E=0 occasionally,
            //having particle code 88 (unrecognized by PDG), for documentation purposes.
            //Even ROOT's TParticle::Y() is not prepared to deal with that exception!
            //Note that TParticle::Pt() is immune (that would just return 0)...
            //Though granted that that should be extremely rare in this precise condition...
            if( TMath::Abs(fTreeVariablePID) == 3122 || fTreeVariablePID==310 ) {
                fTreeVariableRapMC = pThisV0->Y(); //Perfect Y
            }
            if( lMCstack->IsPhysicalPrimary       (lblMotherPosV0Dghter) ) fTreeVariablePrimaryStatus = 1; //Is Primary!
            if( lMCstack->IsSecondaryFromWeakDecay(lblMotherPosV0Dghter) ) fTreeVariablePrimaryStatus = 2; //Weak Decay!
            if( lMCstack->IsSecondaryFromMaterial (lblMotherPosV0Dghter) ) fTreeVariablePrimaryStatus = 3; //Material Int!
            
            //Now we try to acquire the V0 parent particle, if possible
            Int_t lblThisV0Parent = pThisV0->GetFirstMother();
            if ( lblThisV0Parent > -1 ) { //if it has a parent, get it and store specs
                TParticle* pThisV0Parent = lMCstack->Particle( lblThisV0Parent );
                fTreeVariablePIDMother   = pThisV0Parent->GetPdgCode(); //V0 Mother PDG
                fTreeVariablePtMother    = pThisV0Parent->Pt();         //V0 Mother Pt
                //NOTE: Fill only for charged xi
                if ( TMath::Abs(fTreeVariablePIDMother)==3312) fTreeVariableRapMother   = pThisV0Parent->Y();         //V0 Mother Pt
                //Primary Status for the V0 Mother particle
                if( lMCstack->IsPhysicalPrimary       (lblThisV0Parent) ) fTreeVariablePrimaryStatusMother = 1; //Is Primary!
                if( lMCstack->IsSecondaryFromWeakDecay(lblThisV0Parent) ) fTreeVariablePrimaryStatusMother = 2; //Weak Decay!
                if( lMCstack->IsSecondaryFromMaterial (lblThisV0Parent) ) fTreeVariablePrimaryStatusMother = 3; //Material Int!
            }
        }
        
        //------------------------------------------------
        // Fill Tree!
        //------------------------------------------------
        
        // The conditionals are meant to decrease excessive
        // memory usage!
        
        //First Selection: Reject OnFly
        if( lOnFlyStatus == 0 ) {
            //Second Selection: rough 20-sigma band, parametric.
            //K0Short: Enough to parametrize peak broadening with linear function.
            Double_t lUpperLimitK0Short = (5.63707e-01) + (1.14979e-02)*fTreeVariablePt;
            Double_t lLowerLimitK0Short = (4.30006e-01) - (1.10029e-02)*fTreeVariablePt;
            //Lambda: Linear (for higher pt) plus exponential (for low-pt broadening)
            //[0]+[1]*x+[2]*TMath::Exp(-[3]*x)
            Double_t lUpperLimitLambda = (1.13688e+00) + (5.27838e-03)*fTreeVariablePt + (8.42220e-02)*TMath::Exp(-(3.80595e+00)*fTreeVariablePt);
            Double_t lLowerLimitLambda = (1.09501e+00) - (5.23272e-03)*fTreeVariablePt - (7.52690e-02)*TMath::Exp(-(3.46339e+00)*fTreeVariablePt);
            //Do Selection
            if(
               //Case 1: Lambda Selection
               (fTreeVariableInvMassLambda    < lUpperLimitLambda  && fTreeVariableInvMassLambda     > lLowerLimitLambda &&
                (!fkPreselectDedx || (fkPreselectDedx&&TMath::Abs(fTreeVariableNSigmasPosProton) < 7.0 && TMath::Abs(fTreeVariableNSigmasNegPion) < 7.0) ) &&
                (!fkPreselectPID || fTreeVariablePID == 3122 )
                )
               ||
               //Case 2: AntiLambda Selection
               (fTreeVariableInvMassAntiLambda < lUpperLimitLambda  && fTreeVariableInvMassAntiLambda > lLowerLimitLambda &&
                (!fkPreselectDedx || (fkPreselectDedx&&TMath::Abs(fTreeVariableNSigmasNegProton) < 7.0 && TMath::Abs(fTreeVariableNSigmasPosPion) < 7.0) ) &&
                (!fkPreselectPID || fTreeVariablePID == -3122 )
                )
               ||
               //Case 3: K0Short Selection
               (fTreeVariableInvMassK0s        < lUpperLimitK0Short && fTreeVariableInvMassK0s        > lLowerLimitK0Short &&
                (!fkPreselectDedx || (fkPreselectDedx&&TMath::Abs(fTreeVariableNSigmasNegPion)   < 7.0 && TMath::Abs(fTreeVariableNSigmasPosPion) < 7.0) ) &&
                (!fkPreselectPID || fTreeVariablePID == 310 )
                ) ) {
                   //Pre-selection in case this is AA...
                   
                   //Random denial
                   Bool_t lKeepV0 = kTRUE;
                   if(fkDownScaleV0 && ( fRand->Uniform() > fDownScaleFactorV0 )) lKeepV0 = kFALSE;
                   
                   if ( TMath::Abs(fTreeVariableNegEta)<0.8 && TMath::Abs(fTreeVariablePosEta)<0.8 && fkSaveV0Tree && lKeepV0 ) fTreeV0->Fill();
               }
        }
        
        //------------------------------------------------
        // Fill V0 tree over.
        //------------------------------------------------
        
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // Superlight adaptive output mode
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
        //Step 1: Sweep members of the output object TList and fill all of them as appropriate
        Int_t lNumberOfConfigurations = fListV0->GetEntries();
        //AliWarning(Form("[V0 Analyses] Processing different configurations (%i detected)",lNumberOfConfigurations));
        TH3F *histoout                 = 0x0;
        TH3F *histooutfeeddown         = 0x0;
        TProfile *histoProtonProfile         = 0x0;
        AliV0Result *lV0Result = 0x0;
        for(Int_t lcfg=0; lcfg<lNumberOfConfigurations; lcfg++){
            histoout                 = 0x0;
            histooutfeeddown         = 0x0;
            histoProtonProfile       = 0x0;
            
            //Acquire result objects
            lV0Result = (AliV0Result*) fListV0->At(lcfg);
            histoout            = lV0Result->GetHistogram();
            histooutfeeddown    = lV0Result->GetHistogramFeeddown();
            histoProtonProfile  = lV0Result->GetProtonProfile();
            
            Float_t lMass = 0;
            Float_t lRap  = 0;
            Float_t lPDGMass = -1;
            Float_t lNegdEdx = 100;
            Float_t lPosdEdx = 100;
            Int_t lPDGCode = 0;
            Int_t lPDGCodeXiMother = 0;
            Float_t lBaryonMomentum = -0.5;
            Float_t lBaryonPt = -0.5;
            Float_t lBaryondEdxFromProton = 0;
            Float_t lBaryonTransvMomMCForG3F = -0.5; //warning: MC perfect for Geant3/fluka
            
            //========================================================================
            //Setting up: Variable V0 CosPA
            Float_t lV0CosPACut = lV0Result -> GetCutV0CosPA();
            Float_t lVarV0CosPApar[5];
            lVarV0CosPApar[0] = lV0Result->GetCutVarV0CosPAExp0Const();
            lVarV0CosPApar[1] = lV0Result->GetCutVarV0CosPAExp0Slope();
            lVarV0CosPApar[2] = lV0Result->GetCutVarV0CosPAExp1Const();
            lVarV0CosPApar[3] = lV0Result->GetCutVarV0CosPAExp1Slope();
            lVarV0CosPApar[4] = lV0Result->GetCutVarV0CosPAConst();
            Float_t lVarV0CosPA = TMath::Cos(
                                             lVarV0CosPApar[0]*TMath::Exp(lVarV0CosPApar[1]*fTreeVariablePt) +
                                             lVarV0CosPApar[2]*TMath::Exp(lVarV0CosPApar[3]*fTreeVariablePt) +
                                             lVarV0CosPApar[4]);
            if( lV0Result->GetCutUseVarV0CosPA() ){
                //Only use if tighter than the non-variable cut
                if( lVarV0CosPA > lV0CosPACut ) lV0CosPACut = lVarV0CosPA;
            }
            //========================================================================
            
            if ( lV0Result->GetMassHypothesis() == AliV0Result::kK0Short     ){
                lMass    = fTreeVariableInvMassK0s;
                lRap     = fTreeVariableRapK0Short;
                lPDGMass = 0.497;
                lNegdEdx = fTreeVariableNSigmasNegPion;
                lPosdEdx = fTreeVariableNSigmasPosPion;
                lPDGCode = 310;
                lBaryonTransvMomMCForG3F = 999; //nonsense (if you see this you should doubt it...)
            }
            if ( lV0Result->GetMassHypothesis() == AliV0Result::kLambda      ){
                lMass = fTreeVariableInvMassLambda;
                lRap = fTreeVariableRapLambda;
                lPDGMass = 1.115683;
                lNegdEdx = fTreeVariableNSigmasNegPion;
                lPosdEdx = fTreeVariableNSigmasPosProton;
                lPDGCode = 3122;
                lPDGCodeXiMother = 3312;
                lBaryonMomentum = fTreeVariablePosInnerP;
                lBaryonPt = lThisPosInnerPt;
                lBaryondEdxFromProton = fTreeVariableNSigmasPosProton;
                lBaryonTransvMomMCForG3F = lMCTransvMomPos; //proton
            }
            if ( lV0Result->GetMassHypothesis() == AliV0Result::kAntiLambda  ){
                lMass = fTreeVariableInvMassAntiLambda;
                lRap = fTreeVariableRapLambda;
                lPDGMass = 1.115683;
                lNegdEdx = fTreeVariableNSigmasNegProton;
                lPosdEdx = fTreeVariableNSigmasPosPion;
                lPDGCode = -3122;
                lPDGCodeXiMother = -3312;
                lBaryonMomentum = fTreeVariableNegInnerP;
                lBaryonPt = lThisNegInnerPt;
                lBaryondEdxFromProton = fTreeVariableNSigmasNegProton;
                lBaryonTransvMomMCForG3F = lMCTransvMomNeg; //antiproton
            }
            
            //Override rapidity for true rapidity if requested to do so
            if ( lV0Result -> GetCutMCUseMCProperties() ) {
                lRap = fTreeVariableRapMC;
            }
            
            if (
                //Check 1: Offline Vertexer
                lOnFlyStatus == lV0Result->GetUseOnTheFly() &&
                
                //Check 2: Basic Acceptance cuts
                lV0Result->GetCutMinEtaTracks() < fTreeVariableNegEta && fTreeVariableNegEta < lV0Result->GetCutMaxEtaTracks() &&
                lV0Result->GetCutMinEtaTracks() < fTreeVariablePosEta && fTreeVariablePosEta < lV0Result->GetCutMaxEtaTracks() &&
                lRap > lV0Result->GetCutMinRapidity() &&
                lRap < lV0Result->GetCutMaxRapidity() &&
                
                //Check 3: Topological Variables
                fTreeVariableV0Radius > lV0Result->GetCutV0Radius() &&
                fTreeVariableV0Radius < lV0Result->GetCutMaxV0Radius() &&
                fTreeVariableDcaNegToPrimVertex > lV0Result->GetCutDCANegToPV() &&
                fTreeVariableDcaPosToPrimVertex > lV0Result->GetCutDCAPosToPV() &&
                fTreeVariableDcaV0Daughters < lV0Result->GetCutDCAV0Daughters() &&
                fTreeVariableV0CosineOfPointingAngle > lV0CosPACut &&
                fTreeVariableDistOverTotMom*lPDGMass < lV0Result->GetCutProperLifetime() &&
                fTreeVariableLeastNbrCrossedRows > lV0Result->GetCutLeastNumberOfCrossedRows() &&
                fTreeVariableLeastRatioCrossedRowsOverFindable > lV0Result->GetCutLeastNumberOfCrossedRowsOverFindable() &&
                
                //Check 4: Minimum momentum of baryon daughter
                ( lV0Result->GetMassHypothesis() == AliV0Result::kK0Short || lBaryonMomentum > lV0Result->GetCutMinBaryonMomentum() ) &&
                
                //Check 5: TPC dEdx selections
                TMath::Abs(lNegdEdx)<lV0Result->GetCutTPCdEdx() &&
                TMath::Abs(lPosdEdx)<lV0Result->GetCutTPCdEdx() &&
                
                //Check 6: Armenteros-Podolanski space cut (for K0Short analysis)
                ( ( lV0Result->GetCutArmenteros() == kFALSE || lV0Result->GetMassHypothesis() != AliV0Result::kK0Short ) || ( fTreeVariablePtArmV0>lV0Result->GetCutArmenterosParameter()*TMath::Abs(fTreeVariableAlphaV0) ) ) &&
                
                //Check 7: kITSrefit track selection if requested
                (
                 ( (fTreeVariableNegTrackStatus & AliESDtrack::kITSrefit) &&
                  (fTreeVariablePosTrackStatus & AliESDtrack::kITSrefit) )
                 ||
                 !lV0Result->GetCutUseITSRefitTracks()
                 )&&
                
                //Check 8: Max Chi2/Clusters if not absurd
                ( lV0Result->GetCutMaxChi2PerCluster()>1e+3 ||
                 fTreeVariableMaxChi2PerCluster < lV0Result->GetCutMaxChi2PerCluster()
                 ) &&
                
                //Check 9: Min Track Length if positive
                ( lV0Result->GetCutMinTrackLength()<0 || //this is a bit paranoid...
                 fTreeVariableMinTrackLength > lV0Result->GetCutMinTrackLength()
                 )&&
                
                //Check 10: Special 2.76TeV-like dedx
                // Logic: either not requested, or K0Short, or high-pT baryon daughter, or passes cut!
                ( !lV0Result->GetCut276TeVLikedEdx() ||
                 ( lV0Result->GetMassHypothesis() == AliV0Result::kK0Short ||
                  ( lBaryonPt > 1.0 || TMath::Abs(lBaryondEdxFromProton)<3.0 )
                  )
                 )
                )
            {
                //Regular fill histogram here
                if (
                    ( ! (lV0Result->GetCutMCPhysicalPrimary())    || fTreeVariablePrimaryStatus == 1 ) &&
                    ( ! (lV0Result->GetCutMCLambdaFromPrimaryXi())|| (fTreeVariablePrimaryStatusMother == 1 && fTreeVariablePIDMother == lPDGCodeXiMother) ) &&
                    ( ! (lV0Result->GetCutMCPDGCodeAssociation()) || fTreeVariablePID == lPDGCode     )
                    ){
                    //This satisfies all my conditionals! Fill histogram
                    if( !lV0Result -> GetCutMCUseMCProperties() ){
                        histoout -> Fill ( fCentrality, fTreeVariablePt, lMass );
                        if(histoProtonProfile)
                            histoProtonProfile -> Fill( fTreeVariablePt, lBaryonTransvMomMCForG3F );
                    }else{
                        histoout -> Fill ( fCentrality, fTreeVariablePtMC, lMass );
                        if(histoProtonProfile)
                            histoProtonProfile -> Fill( fTreeVariablePtMC, lBaryonTransvMomMCForG3F );
                    }
                }
                
                //Fill feeddown matrix, please
                if (
                    histooutfeeddown &&
                    (fTreeVariablePrimaryStatusMother == 1 && fTreeVariablePIDMother == lPDGCodeXiMother) &&
                    (  fTreeVariablePID == lPDGCode     )
                    ){
                    //Warning: has to be filled with perfect properties
                    //Rough invariant mass selection: could be better, but would be a correction
                    //of the correction -> left as further improvement
                    if( TMath::Abs(lMass-1.116) < 0.010 )
                        histooutfeeddown -> Fill ( fTreeVariablePt, fTreeVariablePtMother, fCentrality );
                }
            }
        }
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // End Superlight adaptive output mode
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
    }// This is the end of the V0 loop
    
    //------------------------------------------------
    // Rerun cascade vertexer!
    //------------------------------------------------
    
    if( fkRunVertexers ) {
        //Remove existing cascades
        lESDevent->ResetCascades();
        
        //Decide between regular and light vertexer (default: light)
        if ( ! fkUseLightVertexer ){
            //Instantiate vertexer object
            AliCascadeVertexer lCascVtxer;
            lCascVtxer.SetDefaultCuts(fCascadeVertexerSels);
            lCascVtxer.SetCuts(fCascadeVertexerSels);
            lCascVtxer.V0sTracks2CascadeVertices(lESDevent);
        } else {
            AliLightCascadeVertexer lCascVtxer;
            lCascVtxer.SetDefaultCuts(fCascadeVertexerSels);
            lCascVtxer.SetCuts(fCascadeVertexerSels);
            if( fkUseOnTheFlyV0Cascading ) lCascVtxer.SetUseOnTheFlyV0(kTRUE);
            lCascVtxer.V0sTracks2CascadeVertices(lESDevent);
        }
    }
    
    
    //-----------------------------------------------
    // Run on V0's to look for Siblings
    //-----------------------------------------------
    
    // stores relevant tracks in another array
    Int_t nentr=(Int_t)lESDevent->GetNumberOfTracks();
    TArrayI IdxForSibTagging(nentr); Int_t ntr=0;
    
    for (Int_t iV0 = 0; iV0 < nv0s; iV0++) //Looping on v0s for sibling tagging
    {   // This is the begining of the V0 loop
        AliESDv0 *v0 = ((AliESDEvent*)lESDevent)->GetV0(iV0);
        if (!v0) continue;
        
        //Skip on-the-fly V0s, use only offline for now
        if ( v0->GetOnFlyStatus() ) continue;
        
        CheckChargeV0( v0 );
        //Remove like-sign (will not affect offline V0 candidates!)
        if( v0->GetParamN()->Charge() > 0 && v0->GetParamP()->Charge() > 0 ){
            continue;
        }
        if( v0->GetParamN()->Charge() < 0 && v0->GetParamP()->Charge() < 0 ){
            continue;
        }
        
        Double_t tDecayVertexV0[3];
        v0->GetXYZ(tDecayVertexV0[0],tDecayVertexV0[1],tDecayVertexV0[2]);
        
        Double_t tV0mom[3];
        v0->GetPxPyPz( tV0mom[0],tV0mom[1],tV0mom[2] );
        Double_t lV0TotalMomentum = TMath::Sqrt(
                                                tV0mom[0]*tV0mom[0]+tV0mom[1]*tV0mom[1]+tV0mom[2]*tV0mom[2] );
        
        lV0Radius = TMath::Sqrt(tDecayVertexV0[0]*tDecayVertexV0[0]+tDecayVertexV0[1]*tDecayVertexV0[1]);
        
        lPt = v0->Pt();
        lRapK0Short = v0->RapK0Short();
        lRapLambda  = v0->RapLambda();
        
        UInt_t lKeyPos = (UInt_t)TMath::Abs(v0->GetPindex());
        UInt_t lKeyNeg = (UInt_t)TMath::Abs(v0->GetNindex());
        
        Double_t lMomPos[3];
        v0->GetPPxPyPz(lMomPos[0],lMomPos[1],lMomPos[2]);
        Double_t lMomNeg[3];
        v0->GetNPxPyPz(lMomNeg[0],lMomNeg[1],lMomNeg[2]);
        
        AliESDtrack *pTrack=((AliESDEvent*)lESDevent)->GetTrack(lKeyPos);
        AliESDtrack *nTrack=((AliESDEvent*)lESDevent)->GetTrack(lKeyNeg);
        if (!pTrack || !nTrack) {
            Printf("ERROR: Could not retreive one of the daughter track");
            continue;
        }
        fTreeVariablePosPIDForTracking = pTrack->GetPIDForTracking();
        fTreeVariableNegPIDForTracking = nTrack->GetPIDForTracking();
        
        const AliExternalTrackParam *innernegv0=nTrack->GetInnerParam();
        const AliExternalTrackParam *innerposv0=pTrack->GetInnerParam();
        Float_t lThisPosInnerP = -1;
        Float_t lThisNegInnerP = -1;
        if(innerposv0)  { lThisPosInnerP  = innerposv0 ->GetP(); }
        if(innernegv0)  { lThisNegInnerP  = innernegv0 ->GetP(); }
        Float_t lThisPosdEdx = pTrack -> GetTPCsignal();
        Float_t lThisNegdEdx = nTrack -> GetTPCsignal();
        
        // Filter like-sign V0 (next: add counter and distribution)
        if ( pTrack->GetSign() == nTrack->GetSign()) {
            continue;
        }
        
        //________________________________________________________________________
        // Track quality cuts
        Float_t lPosTrackCrossedRows = pTrack->GetTPCClusterInfo(2,1);
        Float_t lNegTrackCrossedRows = nTrack->GetTPCClusterInfo(2,1);
        fTreeVariableLeastNbrCrossedRows = (Int_t) lPosTrackCrossedRows;
        if( lNegTrackCrossedRows < fTreeVariableLeastNbrCrossedRows )
            fTreeVariableLeastNbrCrossedRows = (Int_t) lNegTrackCrossedRows;
        
        // TPC refit condition (done during reconstruction for Offline but not for On-the-fly)
        if( !(pTrack->GetStatus() & AliESDtrack::kTPCrefit)) continue;
        if( !(nTrack->GetStatus() & AliESDtrack::kTPCrefit)) continue;
        
        //GetKinkIndex condition
        if( pTrack->GetKinkIndex(0)>0 || nTrack->GetKinkIndex(0)>0 ) continue;
        
        //Findable clusters > 0 condition
        if( pTrack->GetTPCNclsF()<=0 || nTrack->GetTPCNclsF()<=0 ) continue;
        
        //Compute ratio Crossed Rows / Findable clusters
        //Note: above test avoids division by zero!
        Float_t lPosTrackCrossedRowsOverFindable = lPosTrackCrossedRows / ((double)(pTrack->GetTPCNclsF()));
        Float_t lNegTrackCrossedRowsOverFindable = lNegTrackCrossedRows / ((double)(nTrack->GetTPCNclsF()));
        
        fTreeVariableLeastRatioCrossedRowsOverFindable = lPosTrackCrossedRowsOverFindable;
        if( lNegTrackCrossedRowsOverFindable < fTreeVariableLeastRatioCrossedRowsOverFindable )
            fTreeVariableLeastRatioCrossedRowsOverFindable = lNegTrackCrossedRowsOverFindable;
        
        //Lowest Cut Level for Ratio Crossed Rows / Findable = 0.8, set here
        if ( fTreeVariableLeastRatioCrossedRowsOverFindable < 0.8 ) continue;
        
        //Extra track quality: Chi2/cluster for cross-checks
        Float_t lBiggestChi2PerCluster = -1;
        
        Float_t lPosChi2PerCluster = 1000;
        Float_t lNegChi2PerCluster = 1000;
        
        if( pTrack->GetTPCNcls() > 0 ) lPosChi2PerCluster = pTrack->GetTPCchi2() / ((Float_t)pTrack->GetTPCNcls());
        if( nTrack->GetTPCNcls() > 0 ) lNegChi2PerCluster = nTrack->GetTPCchi2() / ((Float_t)nTrack->GetTPCNcls());
        
        if ( lPosChi2PerCluster  > lBiggestChi2PerCluster ) lBiggestChi2PerCluster = lPosChi2PerCluster;
        if ( lNegChi2PerCluster  > lBiggestChi2PerCluster ) lBiggestChi2PerCluster = lNegChi2PerCluster;
        
        fTreeVariableMaxChi2PerCluster = lBiggestChi2PerCluster;
        
        //Extra track quality: min track length
        Float_t lSmallestTrackLength = 1000;
        Float_t lPosTrackLength = -1;
        Float_t lNegTrackLength = -1;
        
        if (pTrack->GetInnerParam()) lPosTrackLength = pTrack->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
        if (nTrack->GetInnerParam()) lNegTrackLength = nTrack->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
        
        if ( lPosTrackLength  < lSmallestTrackLength ) lSmallestTrackLength = lPosTrackLength;
        if ( lNegTrackLength  < lSmallestTrackLength ) lSmallestTrackLength = lNegTrackLength;
        
        fTreeVariableMinTrackLength = lSmallestTrackLength;
        
        if ( ( ( ( pTrack->GetTPCClusterInfo(2,1) ) < 70 ) || ( ( nTrack->GetTPCClusterInfo(2,1) ) < 70 ) ) && lSmallestTrackLength<80 ) continue;
        
        //End track Quality Cuts
        //________________________________________________________________________
        
        lDcaPosToPrimVertex = TMath::Abs(pTrack->GetD(lBestPrimaryVtxPos[0],
                                                      lBestPrimaryVtxPos[1],
                                                      lMagneticField) );
        
        lDcaNegToPrimVertex = TMath::Abs(nTrack->GetD(lBestPrimaryVtxPos[0],
                                                      lBestPrimaryVtxPos[1],
                                                      lMagneticField) );
        
        lDcaV0Daughters = v0->GetDcaV0Daughters();
        lDcaV0ToPrimVertex = v0->GetD(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2]);
        lV0CosineOfPointingAngle = v0->GetV0CosineOfPointingAngle(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2]);
        
        // Getting invariant mass infos directly from ESD
        v0->ChangeMassHypothesis(310);
        lInvMassK0s = v0->GetEffMass();
        v0->ChangeMassHypothesis(3122);
        lInvMassLambda = v0->GetEffMass();
        v0->ChangeMassHypothesis(-3122);
        lInvMassAntiLambda = v0->GetEffMass();
        
        //Here is the time to make cut selections!
        
        if( lV0CosineOfPointingAngle < fSibCutV0CosineOfPointingAngle) continue ;
        if( lDcaV0ToPrimVertex       > fSibCutDcaV0ToPrimVertex      ) continue ;
//        if( lDcaV0Daughters          > lSibCutDcaV0Daughters         ) continue ;
        if( lV0Radius                > fSibCutV0Radius               ) continue ;
        if( lDcaPosToPrimVertex      > fSibCutDcaPosToPrimVertex     ) continue ;
        if( lDcaNegToPrimVertex      > fSibCutDcaNegToPrimVertex     ) continue ;
        if( TMath::Abs( lInvMassK0s - 0.498 ) > fSibCutInvMassK0s    ) continue ;
        //This V0 looks like one true K0s!
        
        //Official means of acquiring N-sigmas
        Float_t NSigmasPosProton = TMath::Abs( fPIDResponse->NumberOfSigmasTPC( pTrack, AliPID::kProton ) );
        Float_t NSigmasPosPion   = TMath::Abs( fPIDResponse->NumberOfSigmasTPC( pTrack, AliPID::kPion ) );
        Float_t NSigmasNegProton = TMath::Abs( fPIDResponse->NumberOfSigmasTPC( nTrack, AliPID::kProton ) );
        Float_t NSigmasNegPion   = TMath::Abs( fPIDResponse->NumberOfSigmasTPC( nTrack, AliPID::kPion ) );
        
        Float_t DeltaMassKaon = TMath::Abs( lInvMassK0s - .498 );
        Float_t DeltaMassLamb = TMath::Abs( lInvMassLambda - 1.116);
        Float_t DeltaMassALam = TMath::Abs( lInvMassAntiLambda - 1.116);
        
        if( ( NSigmasPosPion < 4 ) && ( NSigmasNegPion < 4 ) ){
            //        if(1){
            //This is a good V0 for Sibling Ancestor, testing if already on the list
            Bool_t PosFlag = 0 ;
            Bool_t NegFlag = 0 ;
            
            for(Int_t t = 0 ; t < ntr ; t++ ){
                if(lKeyPos == IdxForSibTagging[t]) PosFlag = 1;
                if(lKeyNeg == IdxForSibTagging[t]) NegFlag = 1;
            }
            if(PosFlag == 0 ) IdxForSibTagging[ntr++] = lKeyPos ;
            if(NegFlag == 0 ) IdxForSibTagging[ntr++] = lKeyNeg ;
        }
    }
    
    // End of V0 loop for Siblings
    
    //------------------------------------------------
    // MAIN CASCADE LOOP STARTS HERE
    //------------------------------------------------
    // Code Credit: Antonin Maire (thanks^100)
    // ---> This is an adaptation
    
    Long_t ncascades = 0;
    ncascades = lESDevent->GetNumberOfCascades();
    
    for (Int_t iXi = 0; iXi < ncascades; iXi++) {
        //------------------------------------------------
        // Initializations
        //------------------------------------------------
        //Double_t lTrkgPrimaryVtxRadius3D = -500.0;
        //Double_t lBestPrimaryVtxRadius3D = -500.0;
        fTreeCascVarNegPx = 0.0;
        fTreeCascVarNegPy = 0.0;
        fTreeCascVarNegPz = 0.0;
        fTreeCascVarPosPx = 0.0;
        fTreeCascVarPosPy = 0.0;
        fTreeCascVarPosPz = 0.0;
        fTreeCascVarBachPx = 0.0;
        fTreeCascVarBachPy = 0.0;
        fTreeCascVarBachPz = 0.0;
        fTreeCascVarNegPxMC = 0.0;
        fTreeCascVarNegPyMC = 0.0;
        fTreeCascVarNegPzMC = 0.0;
        fTreeCascVarPosPxMC = 0.0;
        fTreeCascVarPosPyMC = 0.0;
        fTreeCascVarPosPzMC = 0.0;
        fTreeCascVarBachPxMC = 0.0;
        fTreeCascVarBachPyMC = 0.0;
        fTreeCascVarBachPzMC = 0.0;
        
        fTreeCascVarBachelorDCAptX = -100; //!
        fTreeCascVarBachelorDCAptY = -100; //!
        fTreeCascVarBachelorDCAptZ = -100; //!
        fTreeCascVarV0DCAptX = -100; //!
        fTreeCascVarV0DCAptY = -100; //!
        fTreeCascVarV0DCAptZ = -100; //!
        fTreeCascVarDCADaughters_Test = -100;
        fTreeCascVarBachelorDCAptSigmaX2 = -100;
        fTreeCascVarBachelorDCAptSigmaY2 = -100;
        fTreeCascVarBachelorDCAptSigmaZ2 = -100;
        fTreeCascVarV0DCAptUncertainty_V0Pos = -100;
        fTreeCascVarV0DCAptUncertainty_V0Ang = -100;
        
        fTreeCascVarV0DCAptPosSigmaX2 = -100;
        fTreeCascVarV0DCAptPosSigmaY2 = -100;
        fTreeCascVarV0DCAptPosSigmaZ2 = -100;
        fTreeCascVarV0DCAptPosSigmaSnp2 = -100;
        fTreeCascVarV0DCAptPosSigmaTgl2 = -100;
        fTreeCascVarV0DCAptNegSigmaX2 = -100;
        fTreeCascVarV0DCAptNegSigmaY2 = -100;
        fTreeCascVarV0DCAptNegSigmaZ2 = -100;
        fTreeCascVarV0DCAptNegSigmaSnp2 = -100;
        fTreeCascVarV0DCAptNegSigmaTgl2 = -100;
        
        fTreeCascVarNegDCAPVSigmaX2 = 1e+3;
        fTreeCascVarNegDCAPVSigmaY2 = 1e+3;
        fTreeCascVarNegDCAPVSigmaZ2 = 1e+3;
        fTreeCascVarPosDCAPVSigmaX2 = 1e+3;
        fTreeCascVarPosDCAPVSigmaY2 = 1e+3;
        fTreeCascVarPosDCAPVSigmaZ2 = 1e+3;
        fTreeCascVarBachDCAPVSigmaX2 = 1e+3;
        fTreeCascVarBachDCAPVSigmaY2 = 1e+3;
        fTreeCascVarBachDCAPVSigmaZ2 = 1e+3;
        
        fTreeCascVarPosITSClusters0 = 0;
        fTreeCascVarPosITSClusters1 = 0;
        fTreeCascVarPosITSClusters2 = 0;
        fTreeCascVarPosITSClusters3 = 0;
        fTreeCascVarPosITSClusters4 = 0;
        fTreeCascVarPosITSClusters5 = 0;
        
        fTreeCascVarNegITSClusters0 = 0;
        fTreeCascVarNegITSClusters1 = 0;
        fTreeCascVarNegITSClusters2 = 0;
        fTreeCascVarNegITSClusters3 = 0;
        fTreeCascVarNegITSClusters4 = 0;
        fTreeCascVarNegITSClusters5 = 0;
        
        fTreeCascVarBachITSClusters0 = 0;
        fTreeCascVarBachITSClusters1 = 0;
        fTreeCascVarBachITSClusters2 = 0;
        fTreeCascVarBachITSClusters3 = 0;
        fTreeCascVarBachITSClusters4 = 0;
        fTreeCascVarBachITSClusters5 = 0;
        
        fTreeCascVarPosITSSharedClusters0 = 0;
        fTreeCascVarPosITSSharedClusters1 = 0;
        fTreeCascVarPosITSSharedClusters2 = 0;
        fTreeCascVarPosITSSharedClusters3 = 0;
        fTreeCascVarPosITSSharedClusters4 = 0;
        fTreeCascVarPosITSSharedClusters5 = 0;
        
        fTreeCascVarNegITSSharedClusters0 = 0;
        fTreeCascVarNegITSSharedClusters1 = 0;
        fTreeCascVarNegITSSharedClusters2 = 0;
        fTreeCascVarNegITSSharedClusters3 = 0;
        fTreeCascVarNegITSSharedClusters4 = 0;
        fTreeCascVarNegITSSharedClusters5 = 0;
        
        fTreeCascVarBachITSSharedClusters0 = 0;
        fTreeCascVarBachITSSharedClusters1 = 0;
        fTreeCascVarBachITSSharedClusters2 = 0;
        fTreeCascVarBachITSSharedClusters3 = 0;
        fTreeCascVarBachITSSharedClusters4 = 0;
        fTreeCascVarBachITSSharedClusters5 = 0;
        
        //Uncertainty information on mass (from KF) for testing purposes
        fTreeCascVarV0LambdaMassError = 1e+4;
        fTreeCascVarV0AntiLambdaMassError = 1e+4;
        
        fTreeCascVarBachIsKink=kFALSE;
        fTreeCascVarPosIsKink=kFALSE;
        fTreeCascVarNegIsKink=kFALSE;
        
        fTreeCascVarBachV0Tagging = -1;
        fTreeCascVarPosV0Tagging = -1;
        fTreeCascVarNegV0Tagging = -1;
        
        // - 1st part of initialisation : variables needed to store AliESDCascade data members
        Double_t lEffMassXi      = 0. ;
        //Double_t lChi2Xi         = -1. ;
        Double_t lDcaXiDaughters = -1. ;
        Double_t lXiCosineOfPointingAngle = -1. ;
        Double_t lPosXi[3] = { -1000.0, -1000.0, -1000.0 };
        Double_t lXiRadius = -1000. ;
        
        // - 2nd part of initialisation : Nbr of clusters within TPC for the 3 daughter cascade tracks
        Int_t    lPosTPCClusters    = -1; // For ESD only ...//FIXME : wait for availability in AOD
        Int_t    lNegTPCClusters    = -1; // For ESD only ...
        Int_t    lBachTPCClusters   = -1; // For ESD only ...
        
        // - 3rd part of initialisation : about V0 part in cascades
        Double_t lInvMassLambdaAsCascDghter = 0.;
        //Double_t lV0Chi2Xi         = -1. ;
        Double_t lDcaV0DaughtersXi = -1.;
        
        Double_t lDcaBachToPrimVertexXi = -1., lDcaV0ToPrimVertexXi = -1.;
        Double_t lDcaPosToPrimVertexXi  = -1.;
        Double_t lDcaNegToPrimVertexXi  = -1.;
        Double_t lV0CosineOfPointingAngleXi = -1. ;
        Double_t lV0CosineOfPointingAngleXiSpecial = -1. ;
        Double_t lPosV0Xi[3] = { -1000. , -1000., -1000. }; // Position of VO coming from cascade
        Double_t lV0RadiusXi = -1000.0;
        Double_t lV0quality  = 0.;
        
        // - 4th part of initialisation : Effective masses
        Double_t lInvMassXiMinus    = 0.;
        Double_t lInvMassXiPlus     = 0.;
        Double_t lInvMassOmegaMinus = 0.;
        Double_t lInvMassOmegaPlus  = 0.;
        
        fTreeCascVarChiSquareV0      = 1e+3;
        fTreeCascVarChiSquareCascade = 1e+3;
        
        // - 6th part of initialisation : extra info for QA
        Double_t lXiMomX       = 0. , lXiMomY = 0., lXiMomZ = 0.;
        Double_t lXiTransvMom  = 0. ;
        //Double_t lXiTransvMomMC= 0. ;
        Double_t lXiTotMom     = 0. ;
        
        Double_t lBachMomX       = 0., lBachMomY  = 0., lBachMomZ   = 0.;
        //Double_t lBachTransvMom  = 0.;
        //Double_t lBachTotMom     = 0.;
        
        fTreeCascVarNegNSigmaPion   = -100;
        fTreeCascVarNegNSigmaProton = -100;
        fTreeCascVarPosNSigmaPion   = -100;
        fTreeCascVarPosNSigmaProton = -100;
        fTreeCascVarBachNSigmaPion  = -100;
        fTreeCascVarBachNSigmaKaon  = -100;
        
        Short_t  lChargeXi = -2;
        //Double_t lV0toXiCosineOfPointingAngle = 0. ;
        
        Double_t lRapXi   = -20.0, lRapOmega = -20.0, lRapMC = -20.0; //  lEta = -20.0, lTheta = 360., lPhi = 720. ;
        //Double_t lAlphaXi = -200., lPtArmXi  = -200.0;
        
        // -------------------------------------
        // II.ESD - Calculation Part dedicated to Xi vertices (ESD)
        
        AliESDcascade *xi = lESDevent->GetCascade(iXi);
        if (!xi) continue;
        
        // - II.Step 2 : Assigning the necessary variables for specific AliESDcascade data members (ESD)
        //-------------
        lV0quality = 0.;
        xi->ChangeMassHypothesis(lV0quality , 3312); // default working hypothesis : cascade = Xi- decay
        
        lEffMassXi  			= xi->GetEffMassXi();
        
        //ChiSquare implementation
        fTreeCascVarChiSquareV0      = xi->GetChi2V0();
        fTreeCascVarChiSquareCascade = xi->GetChi2Xi();
        
        lDcaXiDaughters 	= xi->GetDcaXiDaughters();
        lXiCosineOfPointingAngle 	            = xi->GetCascadeCosineOfPointingAngle( lBestPrimaryVtxPos[0],
                                                                                      lBestPrimaryVtxPos[1],
                                                                                      lBestPrimaryVtxPos[2] );
        // Take care : the best available vertex should be used (like in AliCascadeVertexer)
        
        xi->GetXYZcascade( lPosXi[0],  lPosXi[1], lPosXi[2] );
        lXiRadius			= TMath::Sqrt( lPosXi[0]*lPosXi[0]  +  lPosXi[1]*lPosXi[1] );
        
        fTreeCascVarCascadeDecayX = lPosXi[0];
        fTreeCascVarCascadeDecayY = lPosXi[1];
        fTreeCascVarCascadeDecayZ = lPosXi[2];
        
        // - II.Step 3 : around the tracks : Bach + V0 (ESD)
        // ~ Necessary variables for ESDcascade data members coming from the ESDv0 part (inheritance)
        //-------------
        
        UInt_t lIdxPosXi 	= (UInt_t) TMath::Abs( xi->GetPindex() );
        UInt_t lIdxNegXi 	= (UInt_t) TMath::Abs( xi->GetNindex() );
        UInt_t lBachIdx 	= (UInt_t) TMath::Abs( xi->GetBindex() );
        // Care track label can be negative in MC production (linked with the track quality)
        // However = normally, not the case for track index ...
        
        // FIXME : rejection of a double use of a daughter track (nothing but just a crosscheck of what is done in the cascade vertexer)
        if(lBachIdx == lIdxNegXi) {
            AliWarning("Pb / Idx(Bach. track) = Idx(Neg. track) ... continue!");
            continue;
        }
        if(lBachIdx == lIdxPosXi) {
            AliWarning("Pb / Idx(Bach. track) = Idx(Pos. track) ... continue!");
            continue;
        }
        
        AliESDtrack *pTrackXi		= lESDevent->GetTrack( lIdxPosXi );
        AliESDtrack *nTrackXi		= lESDevent->GetTrack( lIdxNegXi );
        AliESDtrack *bachTrackXi	= lESDevent->GetTrack( lBachIdx );
        
        //Sandbox information: always, regardless of status
        fTreeCascVarBachTrack = bachTrackXi;
        fTreeCascVarPosTrack = pTrackXi;
        fTreeCascVarNegTrack = nTrackXi;
        
        fTreeCascVarMagneticField = lESDevent->GetMagneticField(); 
        
        fTreeCascVarNegIndex  = lIdxNegXi;
        fTreeCascVarPosIndex  = lIdxPosXi;
        fTreeCascVarBachIndex = lBachIdx;
        
        //Tagging of True K0's dau
        Bool_t lBachV0Tag = 0 , lNegV0Tag = 0 , lPosV0Tag = 0;
        for(Int_t t = 0 ; t < ntr ; t ++ ){
            if( lBachIdx == IdxForSibTagging[t] ) lBachV0Tag = 1;
            if( lIdxNegXi == IdxForSibTagging[t] ) lNegV0Tag = 1;
            if( lIdxPosXi == IdxForSibTagging[t] ) lPosV0Tag = 1;
        }
        fTreeCascVarBachV0Tagging = lBachV0Tag;
        fTreeCascVarNegV0Tagging = lNegV0Tag;
        fTreeCascVarPosV0Tagging = lPosV0Tag;
        
        if (!pTrackXi || !nTrackXi || !bachTrackXi ) {
            AliWarning("ERROR: Could not retrieve one of the 3 ESD daughter tracks of the cascade ...");
            continue;
        }
        
        //Check its clusters
        fTreeCascVarPosITSClusters0 = pTrackXi->HasPointOnITSLayer(0);
        fTreeCascVarPosITSClusters1 = pTrackXi->HasPointOnITSLayer(1);
        fTreeCascVarPosITSClusters2 = pTrackXi->HasPointOnITSLayer(2);
        fTreeCascVarPosITSClusters3 = pTrackXi->HasPointOnITSLayer(3);
        fTreeCascVarPosITSClusters4 = pTrackXi->HasPointOnITSLayer(4);
        fTreeCascVarPosITSClusters5 = pTrackXi->HasPointOnITSLayer(5);
        
        fTreeCascVarNegITSClusters0 = nTrackXi->HasPointOnITSLayer(0);
        fTreeCascVarNegITSClusters1 = nTrackXi->HasPointOnITSLayer(1);
        fTreeCascVarNegITSClusters2 = nTrackXi->HasPointOnITSLayer(2);
        fTreeCascVarNegITSClusters3 = nTrackXi->HasPointOnITSLayer(3);
        fTreeCascVarNegITSClusters4 = nTrackXi->HasPointOnITSLayer(4);
        fTreeCascVarNegITSClusters5 = nTrackXi->HasPointOnITSLayer(5);
        
        fTreeCascVarBachITSClusters0 = bachTrackXi->HasPointOnITSLayer(0);
        fTreeCascVarBachITSClusters1 = bachTrackXi->HasPointOnITSLayer(1);
        fTreeCascVarBachITSClusters2 = bachTrackXi->HasPointOnITSLayer(2);
        fTreeCascVarBachITSClusters3 = bachTrackXi->HasPointOnITSLayer(3);
        fTreeCascVarBachITSClusters4 = bachTrackXi->HasPointOnITSLayer(4);
        fTreeCascVarBachITSClusters5 = bachTrackXi->HasPointOnITSLayer(5);
        
        //Check its clusters, shared
        fTreeCascVarPosITSSharedClusters0 = pTrackXi->HasSharedPointOnITSLayer(0);
        fTreeCascVarPosITSSharedClusters1 = pTrackXi->HasSharedPointOnITSLayer(1);
        fTreeCascVarPosITSSharedClusters2 = pTrackXi->HasSharedPointOnITSLayer(2);
        fTreeCascVarPosITSSharedClusters3 = pTrackXi->HasSharedPointOnITSLayer(3);
        fTreeCascVarPosITSSharedClusters4 = pTrackXi->HasSharedPointOnITSLayer(4);
        fTreeCascVarPosITSSharedClusters5 = pTrackXi->HasSharedPointOnITSLayer(5);
        
        fTreeCascVarNegITSSharedClusters0 = nTrackXi->HasSharedPointOnITSLayer(0);
        fTreeCascVarNegITSSharedClusters1 = nTrackXi->HasSharedPointOnITSLayer(1);
        fTreeCascVarNegITSSharedClusters2 = nTrackXi->HasSharedPointOnITSLayer(2);
        fTreeCascVarNegITSSharedClusters3 = nTrackXi->HasSharedPointOnITSLayer(3);
        fTreeCascVarNegITSSharedClusters4 = nTrackXi->HasSharedPointOnITSLayer(4);
        fTreeCascVarNegITSSharedClusters5 = nTrackXi->HasSharedPointOnITSLayer(5);
        
        fTreeCascVarBachITSSharedClusters0 = bachTrackXi->HasSharedPointOnITSLayer(0);
        fTreeCascVarBachITSSharedClusters1 = bachTrackXi->HasSharedPointOnITSLayer(1);
        fTreeCascVarBachITSSharedClusters2 = bachTrackXi->HasSharedPointOnITSLayer(2);
        fTreeCascVarBachITSSharedClusters3 = bachTrackXi->HasSharedPointOnITSLayer(3);
        fTreeCascVarBachITSSharedClusters4 = bachTrackXi->HasSharedPointOnITSLayer(4);
        fTreeCascVarBachITSSharedClusters5 = bachTrackXi->HasSharedPointOnITSLayer(5);
        
        //Get error parametrization (warning: be careful with offline/on-the-fly differences
        fTreeCascVarV0LambdaMassError = xi->GetKFInfo(4,2,1);
        fTreeCascVarV0AntiLambdaMassError = xi->GetKFInfo(2,4,1);
        
        //GetKinkIndex condition
        if( bachTrackXi->GetKinkIndex(0)>0 ) fTreeCascVarBachIsKink = kTRUE;
        if( pTrackXi->GetKinkIndex(0)>0 ) fTreeCascVarPosIsKink = kTRUE;
        if( nTrackXi->GetKinkIndex(0)>0 ) fTreeCascVarNegIsKink = kTRUE;
        
        //Get track uncertainties
        //WARNING: THIS REFERS TO THE UNCERTAINTIES CLOSEST TO THE PV
        fTreeCascVarNegDCAPVSigmaX2 = TMath::Power(TMath::Sin(nTrackXi->GetAlpha()),2)*nTrackXi->GetSigmaY2();
        fTreeCascVarNegDCAPVSigmaY2 = TMath::Power(TMath::Cos(nTrackXi->GetAlpha()),2)*nTrackXi->GetSigmaY2();
        fTreeCascVarNegDCAPVSigmaZ2 = nTrackXi->GetSigmaZ2();
        
        fTreeCascVarPosDCAPVSigmaX2 = TMath::Power(TMath::Sin(pTrackXi->GetAlpha()),2)*pTrackXi->GetSigmaY2();
        fTreeCascVarPosDCAPVSigmaY2 = TMath::Power(TMath::Cos(pTrackXi->GetAlpha()),2)*pTrackXi->GetSigmaY2();
        fTreeCascVarPosDCAPVSigmaZ2 = pTrackXi->GetSigmaZ2();
        
        fTreeCascVarBachDCAPVSigmaX2 = TMath::Power(TMath::Sin(bachTrackXi->GetAlpha()),2)*bachTrackXi->GetSigmaY2();
        fTreeCascVarBachDCAPVSigmaY2 = TMath::Power(TMath::Cos(bachTrackXi->GetAlpha()),2)*bachTrackXi->GetSigmaY2();
        fTreeCascVarBachDCAPVSigmaZ2 = bachTrackXi->GetSigmaZ2();
        
        fTreeCascVarPosEta = pTrackXi->Eta();
        fTreeCascVarNegEta = nTrackXi->Eta();
        fTreeCascVarBachEta = bachTrackXi->Eta();
        
        Double_t lBMom[3], lNMom[3], lPMom[3];
        xi->GetBPxPyPz( lBMom[0], lBMom[1], lBMom[2] );
        xi->GetPPxPyPz( lPMom[0], lPMom[1], lPMom[2] );
        xi->GetNPxPyPz( lNMom[0], lNMom[1], lNMom[2] );
        
        //\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
        //\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
        if(fkDebugBump){
            //Estimation of relevant positions
            Double_t lBachDCApt[3], lV0DCApt[3];
            
            //_____________________________________________________________________________
            //Need basic re-calculation of cascade characteristics: let's re-combine V0
            Double_t xn, xp, dca=nTrackXi->GetDCA(pTrackXi,lMagneticField,xn,xp);
            AliExternalTrackParam nt(*nTrackXi), pt(*pTrackXi);
            Bool_t corrected=kFALSE;
            if ((nt.GetX() > 3.) && (xn < 3.)) {
                //correct for the beam pipe material
                corrected=kTRUE;
            }
            if ((pt.GetX() > 3.) && (xp < 3.)) {
                //correct for the beam pipe material
                corrected=kTRUE;
            }
            if (corrected) {
                dca=nt.GetDCA(&pt,lMagneticField,xn,xp);
            }
            nt.PropagateTo(xn,lMagneticField); pt.PropagateTo(xp,lMagneticField);
            
            //_____________________________________________________________________________
            //Get uncertainties in V0 decay point
            //POSITIVE
            Double_t alphaPos=pt.GetAlpha(), csp=TMath::Cos(alphaPos), snp=TMath::Sin(alphaPos);
            Double_t sxp=snp*snp*pt.GetSigmaY2()+0.0005*0.0005, syp=csp*csp*pt.GetSigmaY2()+0.0005*0.0005;
            fTreeCascVarV0DCAptPosSigmaX2 = sxp;
            fTreeCascVarV0DCAptPosSigmaY2 = syp;
            fTreeCascVarV0DCAptPosSigmaZ2 = pt.GetSigmaZ2();
            fTreeCascVarV0DCAptPosSigmaSnp2 = pt.GetSigmaSnp2();
            fTreeCascVarV0DCAptPosSigmaTgl2 = pt.GetSigmaTgl2();
            
            //NEGATIVE
            Double_t alphaNeg=nt.GetAlpha(), csn=TMath::Cos(alphaNeg), snn=TMath::Sin(alphaNeg);
            Double_t sxn=snn*snn*nt.GetSigmaY2()+0.0005*0.0005, syn=csn*csn*nt.GetSigmaY2()+0.0005*0.0005;
            fTreeCascVarV0DCAptNegSigmaX2 = sxn;
            fTreeCascVarV0DCAptNegSigmaY2 = syn;
            fTreeCascVarV0DCAptNegSigmaZ2 = nt.GetSigmaZ2();
            fTreeCascVarV0DCAptNegSigmaSnp2 = nt.GetSigmaSnp2();
            fTreeCascVarV0DCAptNegSigmaTgl2 = nt.GetSigmaTgl2();
            
            //_____________________________________________________________________________
            //Recreate V0
            AliESDv0 vertex(nt,lIdxNegXi,pt,lIdxPosXi);
            Float_t cpa=vertex.GetV0CosineOfPointingAngle(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2]);
            vertex.SetDcaV0Daughters(dca);
            vertex.SetV0CosineOfPointingAngle(cpa);
            vertex.ChangeMassHypothesis(310);
            
            //_____________________________________________________________________________
            //V0 re-estimated, proceed to calculating cascade decay vertex
            AliESDv0 *pv0=&vertex;
            AliExternalTrackParam bt(*bachTrackXi), *pbt=&bt;
            Double_t dcaCascade=PropagateToDCA(pv0,pbt,lESDevent, lMagneticField); //propagate call
            fTreeCascVarDCADaughters_Test = dcaCascade;
            
            //_____________________________________________________________________________
            //Concluded, now calculating relevant positions
            Double_t r[3]; pbt->GetXYZ(r);
            Double_t x1=r[0], y1=r[1], z1=r[2]; // position of the bachelor
            Double_t p[3]; pbt->GetPxPyPz(p);
            Double_t px1=p[0], py1=p[1], pz1=p[2];// momentum of the bachelor track
            
            Double_t x2,y2,z2;          // position of the V0
            pv0->GetXYZ(x2,y2,z2);
            Double_t px2,py2,pz2;       // momentum of V0
            pv0->GetPxPyPz(px2,py2,pz2);
            
            Double_t a2=((x1-x2)*px2+(y1-y2)*py2+(z1-z2)*pz2)/(px2*px2+py2*py2+pz2*pz2);
            
            Double_t xm=x2+a2*px2;
            Double_t ym=y2+a2*py2;
            Double_t zm=z2+a2*pz2;
            
            fTreeCascVarBachelorDCAptX=x1;
            fTreeCascVarBachelorDCAptY=y1;
            fTreeCascVarBachelorDCAptZ=z1;
            fTreeCascVarV0DCAptX=xm;
            fTreeCascVarV0DCAptY=ym;
            fTreeCascVarV0DCAptZ=zm;
            
            Double_t alphaBachelor=pbt->GetAlpha(), cs=TMath::Cos(alphaBachelor), sn=TMath::Sin(alphaBachelor);
            //Double_t tmp[3];
            //pbt->GetPxPyPz(tmp);
            //Double_t px1a=tmp[0], py1a=tmp[1], pz1a=tmp[2];
            //pbt->GetXYZ(tmp);
            //Double_t  x1a=tmp[0],  y1a=tmp[1],  z1a=tmp[2];
            const Double_t ss=0.0005*0.0005;//a kind of a residual misalignment precision
            Double_t sx1=sn*sn*pbt->GetSigmaY2()+ss, sy1=cs*cs*pbt->GetSigmaY2()+ss;
            
            fTreeCascVarBachelorDCAptSigmaX2 = sx1;
            fTreeCascVarBachelorDCAptSigmaY2 = sy1;
            fTreeCascVarBachelorDCAptSigmaZ2 = pbt->GetSigmaZ2();
            
            fTreeCascVarV0DCAptUncertainty_V0Pos = pv0->GetSigmaD0();
            fTreeCascVarV0DCAptUncertainty_V0Ang = pv0->GetSigmaAP0();
        }
        //\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
        //\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/
        
        //fTreeCascVarBachTransMom = TMath::Sqrt( lBMom[0]*lBMom[0] + lBMom[1]*lBMom[1] );
        //fTreeCascVarPosTransMom  = TMath::Sqrt( lPMom[0]*lPMom[0] + lPMom[1]*lPMom[1] );
        //fTreeCascVarNegTransMom  = TMath::Sqrt( lNMom[0]*lNMom[0] + lNMom[1]*lNMom[1] );
        
        fTreeCascVarNegPx = lNMom[0];
        fTreeCascVarNegPy = lNMom[1];
        fTreeCascVarNegPz = lNMom[2];
        fTreeCascVarPosPx = lPMom[0];
        fTreeCascVarPosPy = lPMom[1];
        fTreeCascVarPosPz = lPMom[2];
        fTreeCascVarBachPx = lBMom[0];
        fTreeCascVarBachPy = lBMom[1];
        fTreeCascVarBachPz = lBMom[2];
        
        //------------------------------------------------
        // TPC dEdx information
        //------------------------------------------------
        fTreeCascVarNegNSigmaPion   = fPIDResponse->NumberOfSigmasTPC( nTrackXi, AliPID::kPion   );
        fTreeCascVarNegNSigmaProton = fPIDResponse->NumberOfSigmasTPC( nTrackXi, AliPID::kProton );
        fTreeCascVarPosNSigmaPion   = fPIDResponse->NumberOfSigmasTPC( pTrackXi, AliPID::kPion );
        fTreeCascVarPosNSigmaProton = fPIDResponse->NumberOfSigmasTPC( pTrackXi, AliPID::kProton );
        fTreeCascVarBachNSigmaPion  = fPIDResponse->NumberOfSigmasTPC( bachTrackXi, AliPID::kPion );
        fTreeCascVarBachNSigmaKaon  = fPIDResponse->NumberOfSigmasTPC( bachTrackXi, AliPID::kKaon );
        
        //------------------------------------------------
        // Raw TPC dEdx + PIDForTracking information
        //------------------------------------------------
        
        //Step 1: Acquire TPC inner wall total momentum
        const AliExternalTrackParam *innerneg=nTrackXi->GetInnerParam();
        const AliExternalTrackParam *innerpos=pTrackXi->GetInnerParam();
        const AliExternalTrackParam *innerbach=bachTrackXi->GetInnerParam();
        fTreeCascVarPosInnerP = -1;
        fTreeCascVarNegInnerP = -1;
        fTreeCascVarBachInnerP = -1;
        if(innerpos)  { fTreeCascVarPosInnerP  = innerpos ->GetP(); }
        if(innerneg)  { fTreeCascVarNegInnerP  = innerneg ->GetP(); }
        if(innerbach) { fTreeCascVarBachInnerP = innerbach->GetP(); }
        
        //Step 2: Acquire TPC Signals
        fTreeCascVarPosdEdx = pTrackXi->GetTPCsignal();
        fTreeCascVarNegdEdx = nTrackXi->GetTPCsignal();
        fTreeCascVarBachdEdx = bachTrackXi->GetTPCsignal();
        
        //Step 3: Acquire PID For Tracking
        fTreeCascVarPosPIDForTracking = pTrackXi->GetPIDForTracking();
        fTreeCascVarNegPIDForTracking = nTrackXi->GetPIDForTracking();
        fTreeCascVarBachPIDForTracking = bachTrackXi->GetPIDForTracking();
        
        //------------------------------------------------
        // TPC Number of clusters info
        // --- modified to save the smallest number
        // --- of TPC clusters for the 3 tracks
        //------------------------------------------------
        
        lPosTPCClusters   = pTrackXi->GetTPCNcls();
        lNegTPCClusters   = nTrackXi->GetTPCNcls();
        lBachTPCClusters  = bachTrackXi->GetTPCNcls();
        
        // 1 - Poor quality related to TPCrefit
        ULong_t pStatus    = pTrackXi->GetStatus();
        ULong_t nStatus    = nTrackXi->GetStatus();
        ULong_t bachStatus = bachTrackXi->GetStatus();
        
        //fTreeCascVarkITSRefitBachelor = kTRUE;
        //fTreeCascVarkITSRefitNegative = kTRUE;
        //fTreeCascVarkITSRefitPositive = kTRUE;
        
        if ((pStatus&AliESDtrack::kTPCrefit)    == 0) {
            AliDebug(1, "Pb / V0 Pos. track has no TPCrefit ... continue!");
            continue;
        }
        if ((nStatus&AliESDtrack::kTPCrefit)    == 0) {
            AliDebug(1, "Pb / V0 Neg. track has no TPCrefit ... continue!");
            continue;
        }
        if ((bachStatus&AliESDtrack::kTPCrefit) == 0) {
            AliDebug(1, "Pb / Bach.   track has no TPCrefit ... continue!");
            continue;
        }
        
        //Get status flags
        fTreeCascVarPosTrackStatus = pTrackXi->GetStatus();
        fTreeCascVarNegTrackStatus = nTrackXi->GetStatus();
        fTreeCascVarBachTrackStatus = bachTrackXi->GetStatus();
        
        fTreeCascVarPosDCAz = GetDCAz(pTrackXi);
        fTreeCascVarNegDCAz = GetDCAz(nTrackXi);
        fTreeCascVarBachDCAz = GetDCAz(bachTrackXi);
        
        Float_t lPosChi2PerCluster = pTrackXi->GetTPCchi2() / ((Float_t) lPosTPCClusters);
        Float_t lNegChi2PerCluster = nTrackXi->GetTPCchi2() / ((Float_t) lNegTPCClusters);
        Float_t lBachChi2PerCluster = bachTrackXi->GetTPCchi2() / ((Float_t) lBachTPCClusters);
        
        Int_t leastnumberofclusters = 1000;
        Float_t lBiggestChi2PerCluster = -1;
        
        //Pick minimum
        if( lPosTPCClusters < leastnumberofclusters ) leastnumberofclusters = lPosTPCClusters;
        if( lNegTPCClusters < leastnumberofclusters ) leastnumberofclusters = lNegTPCClusters;
        if( lBachTPCClusters < leastnumberofclusters ) leastnumberofclusters = lBachTPCClusters;
        
        //Pick maximum
        if( lPosChi2PerCluster > lBiggestChi2PerCluster ) lBiggestChi2PerCluster = lPosChi2PerCluster;
        if( lNegChi2PerCluster > lBiggestChi2PerCluster ) lBiggestChi2PerCluster = lNegChi2PerCluster;
        if( lBachChi2PerCluster > lBiggestChi2PerCluster ) lBiggestChi2PerCluster = lBachChi2PerCluster;
        
        //Extra track quality: min track length
        Float_t lSmallestTrackLength = 1000;
        Float_t lPosTrackLength = -1;
        Float_t lNegTrackLength = -1;
        Float_t lBachTrackLength = -1;
        
        if (pTrackXi->GetInnerParam()) lPosTrackLength = pTrackXi->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
        if (nTrackXi->GetInnerParam()) lNegTrackLength = nTrackXi->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
        if (bachTrackXi->GetInnerParam()) lBachTrackLength = bachTrackXi->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
        
        if ( lPosTrackLength  < lSmallestTrackLength ) lSmallestTrackLength = lPosTrackLength;
        if ( lNegTrackLength  < lSmallestTrackLength ) lSmallestTrackLength = lNegTrackLength;
        if ( lBachTrackLength  < lSmallestTrackLength ) lSmallestTrackLength = lBachTrackLength;
        
        fTreeCascVarMinTrackLength = lSmallestTrackLength;
        
        // 2 - Poor quality related to TPC clusters: lowest cut of 70 clusters
        if(lPosTPCClusters  < 70 && lSmallestTrackLength < 80) {
            AliDebug(1, "Pb / V0 Pos. track has less than 70 TPC clusters ... continue!");
            continue;
        }
        if(lNegTPCClusters  < 70 && lSmallestTrackLength < 80) {
            AliDebug(1, "Pb / V0 Neg. track has less than 70 TPC clusters ... continue!");
            continue;
        }
        if(lBachTPCClusters < 70 && lSmallestTrackLength < 80) {
            AliDebug(1, "Pb / Bach.   track has less than 70 TPC clusters ... continue!");
            continue;
        }
        
        lInvMassLambdaAsCascDghter	= xi->GetEffMass();
        // This value shouldn't change, whatever the working hyp. is : Xi-, Xi+, Omega-, Omega+
        lDcaV0DaughtersXi 		= xi->GetDcaV0Daughters();
        fTreeCascVarV0ChiSquare = xi->GetChi2V0();
        
        lV0CosineOfPointingAngleXi 	= xi->GetV0CosineOfPointingAngle( lBestPrimaryVtxPos[0],
                                                                     lBestPrimaryVtxPos[1],
                                                                     lBestPrimaryVtxPos[2] );
        //Modification: V0 CosPA wrt to Cascade decay vertex
        lV0CosineOfPointingAngleXiSpecial 	= xi->GetV0CosineOfPointingAngle( lPosXi[0],
                                                                             lPosXi[1],
                                                                             lPosXi[2] );
        
        lDcaV0ToPrimVertexXi 		= xi->GetD( lBestPrimaryVtxPos[0],
                                               lBestPrimaryVtxPos[1],
                                               lBestPrimaryVtxPos[2] );
        
        lDcaBachToPrimVertexXi = TMath::Abs( bachTrackXi->GetD(	lBestPrimaryVtxPos[0],
                                                               lBestPrimaryVtxPos[1],
                                                               lMagneticField  ) );
        // Note : AliExternalTrackParam::GetD returns an algebraic value ...
        
        xi->GetXYZ( lPosV0Xi[0],  lPosV0Xi[1], lPosV0Xi[2] );
        lV0RadiusXi		= TMath::Sqrt( lPosV0Xi[0]*lPosV0Xi[0]  +  lPosV0Xi[1]*lPosV0Xi[1] );
        
        fTreeCascVarV0DecayX = lPosV0Xi[0];
        fTreeCascVarV0DecayY = lPosV0Xi[1];
        fTreeCascVarV0DecayZ = lPosV0Xi[2];
        
        fTreeCascVarPrimVertexX = lBestPrimaryVtxPos[0];
        fTreeCascVarPrimVertexY = lBestPrimaryVtxPos[1];
        fTreeCascVarPrimVertexZ = lBestPrimaryVtxPos[2];
        
        //========================================================================================
        //Calculate V0 lifetime for adaptive decay radius cut
        //3D Distance travelled by the V0 in the cascade
        Float_t lV0DistanceTrav =  TMath::Sqrt(  TMath::Power( lPosV0Xi[0]-lPosXi[0] , 2)
                                               + TMath::Power( lPosV0Xi[1]-lPosXi[1] , 2)
                                               + TMath::Power( lPosV0Xi[2]-lPosXi[2] , 2) );
        
        //Total V0 momentum
        Float_t lV0TotMomentum = TMath::Sqrt(  TMath::Power( lNMom[0]+lPMom[0] , 2)
                                             + TMath::Power( lNMom[1]+lPMom[1] , 2)
                                             + TMath::Power( lNMom[2]+lPMom[2] , 2) );
        
        //V0 transverse momentum
        Float_t lV0Pt = TMath::Sqrt(  TMath::Power( lNMom[0]+lPMom[0] , 2)
                                    + TMath::Power( lNMom[1]+lPMom[1] , 2) );
        
        //Calculate V0 lifetime: mL/p
        if( TMath::Abs(lV0TotMomentum)>1e-5 ){
            fTreeCascVarV0Lifetime = 1.115683*lV0DistanceTrav / lV0TotMomentum;
        }else{
            fTreeCascVarV0Lifetime = -1;
        }
        //========================================================================================
        
        lDcaPosToPrimVertexXi 	= TMath::Abs( pTrackXi	->GetD(	lBestPrimaryVtxPos[0],
                                                               lBestPrimaryVtxPos[1],
                                                               lMagneticField  )     );
        
        lDcaNegToPrimVertexXi 	= TMath::Abs( nTrackXi	->GetD(	lBestPrimaryVtxPos[0],
                                                               lBestPrimaryVtxPos[1],
                                                               lMagneticField  )     );
        
        // - II.Step 4 : around effective masses (ESD)
        // ~ change mass hypotheses to cover all the possibilities :  Xi-/+, Omega -/+
        
        if( bachTrackXi->Charge() < 0 )	{
            lV0quality = 0.;
            xi->ChangeMassHypothesis(lV0quality , 3312);
            // Calculate the effective mass of the Xi- candidate.
            // pdg code 3312 = Xi-
            lInvMassXiMinus = xi->GetEffMassXi();
            
            lV0quality = 0.;
            xi->ChangeMassHypothesis(lV0quality , 3334);
            // Calculate the effective mass of the Xi- candidate.
            // pdg code 3334 = Omega-
            lInvMassOmegaMinus = xi->GetEffMassXi();
            
            lV0quality = 0.;
            xi->ChangeMassHypothesis(lV0quality , 3312); 	// Back to default hyp.
        }// end if negative bachelor
        
        
        if( bachTrackXi->Charge() >  0 ) {
            lV0quality = 0.;
            xi->ChangeMassHypothesis(lV0quality , -3312);
            // Calculate the effective mass of the Xi+ candidate.
            // pdg code -3312 = Xi+
            lInvMassXiPlus = xi->GetEffMassXi();
            
            lV0quality = 0.;
            xi->ChangeMassHypothesis(lV0quality , -3334);
            // Calculate the effective mass of the Xi+ candidate.
            // pdg code -3334  = Omega+
            lInvMassOmegaPlus = xi->GetEffMassXi();
            
            lV0quality = 0.;
            xi->ChangeMassHypothesis(lV0quality , -3312); 	// Back to "default" hyp.
        }// end if positive bachelor
        // - II.Step 6 : extra info for QA (ESD)
        // miscellaneous pieces of info that may help regarding data quality assessment.
        //-------------
        
        xi->GetPxPyPz( lXiMomX, lXiMomY, lXiMomZ );
        lXiTransvMom  	= TMath::Sqrt( lXiMomX*lXiMomX   + lXiMomY*lXiMomY );
        lXiTotMom  	= TMath::Sqrt( lXiMomX*lXiMomX   + lXiMomY*lXiMomY   + lXiMomZ*lXiMomZ );
        
        xi->GetBPxPyPz(  lBachMomX,  lBachMomY,  lBachMomZ );
        //lBachTransvMom  = TMath::Sqrt( lBachMomX*lBachMomX   + lBachMomY*lBachMomY );
        //lBachTotMom  	= TMath::Sqrt( lBachMomX*lBachMomX   + lBachMomY*lBachMomY  +  lBachMomZ*lBachMomZ  );
        
        lChargeXi = xi->Charge();
        
        //lV0toXiCosineOfPointingAngle = xi->GetV0CosineOfPointingAngle( lPosXi[0], lPosXi[1], lPosXi[2] );
        
        lRapXi    = xi->RapXi();
        lRapOmega = xi->RapOmega();
        //lEta      = xi->Eta();
        //lTheta    = xi->Theta() *180.0/TMath::Pi();
        //lPhi      = xi->Phi()   *180.0/TMath::Pi();
        //lAlphaXi  = xi->AlphaXi();
        //lPtArmXi  = xi->PtArmXi();
        
        //----------------------------------------
        // Calculate Cascade DCA to PV, please
        //----------------------------------------
        
        Int_t lChargeCascade = fTreeCascVarCharge;
        
        //cascade properties to get started
        Double_t xyzCascade[3], pxpypzCascade[3], cvCascade[21];
        for(Int_t ii=0;ii<21;ii++) cvCascade[ii]=0.0; //something small
        
        xi->GetXYZcascade( xyzCascade[0],  xyzCascade[1], xyzCascade[2] );
        xi->GetPxPyPz( pxpypzCascade[0], pxpypzCascade[1], pxpypzCascade[2] );
        
        AliExternalTrackParam lCascTrajObject(xyzCascade,pxpypzCascade,cvCascade,lChargeCascade), *hCascTraj = &lCascTrajObject;
        
        Double_t lCascDCAtoPVxy = TMath::Abs(hCascTraj->GetD(lBestPrimaryVtxPos[0],
                                                             lBestPrimaryVtxPos[1],
                                                             lMagneticField) );
        Float_t dzcascade[2];
        hCascTraj->GetDZ(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2], lMagneticField, dzcascade );
        Double_t lCascDCAtoPVz = dzcascade[1];
        
        //assign TTree values
        fTreeCascVarCascDCAtoPVxy = lCascDCAtoPVxy;
        fTreeCascVarCascDCAtoPVz  = lCascDCAtoPVz;
        
        //------------------------------------------------
        // Associate Cascade Candidates to Monte Carlo!
        //------------------------------------------------
        
        //Warning: Not using Continues... Need to fill tree later!
        
        Double_t lXiTransvMomMC= -100. ;
        Int_t lPDGCodeCascade = 0;
        Int_t lPID_BachMother = 0;
        Int_t lPID_NegMother = 0;
        Int_t lPID_PosMother = 0;
        fTreeCascVarIsPhysicalPrimary = 0; // 0: not defined, any candidate may have this
        Double_t fTreeCascVarPosTransvMomentumMC = -1;
        Double_t fTreeCascVarNegTransvMomentumMC = -1;
        fTreeCascVarPIDPositive = -9999;
        fTreeCascVarPIDNegative = -9999;
        fTreeCascVarPIDBachelor = -9999;
        fTreeCascVarPIDPositiveMother = -9999;
        fTreeCascVarPIDNegativeMother = -9999;
        fTreeCascVarPIDBachelorMother = -9999;
        fTreeCascVarPIDPositiveGrandMother = -9999;
        fTreeCascVarPIDNegativeGrandMother = -9999;
        fTreeCascVarPIDBachelorGrandMother = -9999;
        fTreeCascVarBachCousinStatus = -1;
        fTreeCascVarV0BachSibIsValid = -1;
        
        fTreeCascVarNegLabel = -1;
        fTreeCascVarPosLabel = -1;
        fTreeCascVarBachLabel = -1;
        fTreeCascVarNegLabelMother = -1;
        fTreeCascVarPosLabelMother = -1;
        fTreeCascVarBachLabelMother = -1;
        fTreeCascVarNegLabelGrandMother = -1;
        fTreeCascVarPosLabelGrandMother = -1;
        fTreeCascVarBachLabelGrandMother = -1;
        fTreeCascVarV0DecayXMC = -100;
        fTreeCascVarV0DecayYMC = -100;
        fTreeCascVarV0DecayZMC = -100;
        fTreeCascVarCascadeDecayXMC = -100;
        fTreeCascVarCascadeDecayYMC = -100;
        fTreeCascVarCascadeDecayZMC = -100;
        
        fTreeCascVarBachSibPt                     = -1;
        fTreeCascVarBachSibDcaV0ToPrimVertex      = -1;
        fTreeCascVarBachSibDcaV0Daughters         = -1;
        fTreeCascVarBachSibV0CosineOfPointingAngle= -1;
        fTreeCascVarBachSibV0V0Radius             = -1;
        fTreeCascVarBachSibV0DcaPosToPrimVertex   = -1;
        fTreeCascVarBachSibV0DcaNegToPrimVertex   = -1;
        fTreeCascVarBachSibV0InvMassK0s           = -1;
        fTreeCascVarBachSibV0InvMassLambda        = -1;
        fTreeCascVarBachSibV0InvMassAntiLambda    = -1;
        
        fTreeCascVarPosSibPt                     = -1;
        fTreeCascVarPosSibDcaV0ToPrimVertex      = -1;
        fTreeCascVarPosSibDcaV0Daughters         = -1;
        fTreeCascVarPosSibV0CosineOfPointingAngle= -1;
        fTreeCascVarPosSibV0V0Radius             = -1;
        fTreeCascVarPosSibV0DcaPosToPrimVertex   = -1;
        fTreeCascVarPosSibV0DcaNegToPrimVertex   = -1;
        fTreeCascVarPosSibV0InvMassK0s           = -1;
        fTreeCascVarPosSibV0InvMassLambda        = -1;
        fTreeCascVarPosSibV0InvMassAntiLambda    = -1;
        
        fTreeCascVarNegSibPt                     = -1;
        fTreeCascVarNegSibDcaV0ToPrimVertex      = -1;
        fTreeCascVarNegSibDcaV0Daughters         = -1;
        fTreeCascVarNegSibV0CosineOfPointingAngle= -1;
        fTreeCascVarNegSibV0V0Radius             = -1;
        fTreeCascVarNegSibV0DcaPosToPrimVertex   = -1;
        fTreeCascVarNegSibV0DcaNegToPrimVertex   = -1;
        fTreeCascVarNegSibV0InvMassK0s           = -1;
        fTreeCascVarNegSibV0InvMassLambda        = -1;
        fTreeCascVarNegSibV0InvMassAntiLambda    = -1;
        
        if(fDebug > 5)
            cout 	<< "MC EventNumber : " << lMCevent->Header()->GetEvent()
            << " / MC event Number in Run : " << lMCevent->Header()->GetEventNrInRun() << endl;
        
        
        
        //----------------------------------------
        // Bump studies: perform propagation
        //----------------------------------------
        
        AliESDtrack *lBaryonTrack = 0x0;
        AliESDtrack *lBachelorTrack = 0x0;
        if ( lChargeXi == -1 ){
            lBaryonTrack = pTrackXi;
            lBachelorTrack = bachTrackXi;
        }
        if ( lChargeXi == +1 ){
            lBaryonTrack = nTrackXi;
            lBachelorTrack = bachTrackXi;
        }
        
        fTreeCascVarDCABachToBaryon = -100;
        
        Double_t bMag = lESDevent->GetMagneticField();
        Double_t xn, xp;
        
        //Care has to be taken here
        if ( lBaryonTrack && lBachelorTrack ){
            //Attempt zero: Calculate DCA between bachelor and baryon daughter
            fTreeCascVarDCABachToBaryon = lBaryonTrack->GetDCA(lBachelorTrack, bMag, xn, xp);
        }
        
        fTreeCascVarWrongCosPA = -1;
        if( bachTrackXi->Charge() < 0 )
            fTreeCascVarWrongCosPA = GetCosPA( bachTrackXi , pTrackXi, lESDevent );
        if( bachTrackXi->Charge() > 0 )
            fTreeCascVarWrongCosPA = GetCosPA( bachTrackXi , nTrackXi, lESDevent );
        
        //----------------------------------------
        // Regular MC ASSOCIATION STARTS HERE
        //----------------------------------------
        
        Int_t lblPosV0Dghter = (Int_t) TMath::Abs( pTrackXi->GetLabel() );
        // Abs value = needed ! question of quality track association ...
        Int_t lblNegV0Dghter = (Int_t) TMath::Abs( nTrackXi->GetLabel() );
        Int_t lblBach        = (Int_t) TMath::Abs( bachTrackXi->GetLabel() );
        
        fTreeCascVarPosLabel = pTrackXi->GetLabel();
        fTreeCascVarNegLabel = nTrackXi->GetLabel();
        fTreeCascVarBachLabel = bachTrackXi->GetLabel();
        
        TParticle* mcPosV0Dghter = lMCstack->Particle( lblPosV0Dghter );
        TParticle* mcNegV0Dghter = lMCstack->Particle( lblNegV0Dghter );
        TParticle* mcBach        = lMCstack->Particle( lblBach );
        
        //Get MC information
        fTreeCascVarNegPxMC = mcNegV0Dghter->Px();
        fTreeCascVarNegPyMC = mcNegV0Dghter->Py();
        fTreeCascVarNegPzMC = mcNegV0Dghter->Pz();
        fTreeCascVarPosPxMC = mcPosV0Dghter->Px();
        fTreeCascVarPosPyMC = mcPosV0Dghter->Py();
        fTreeCascVarPosPzMC = mcPosV0Dghter->Pz();
        fTreeCascVarBachPxMC = mcBach->Px();
        fTreeCascVarBachPyMC = mcBach->Py();
        fTreeCascVarBachPzMC = mcBach->Pz();
        
        fTreeCascVarIsPhysicalPrimaryNegative = kFALSE;
        fTreeCascVarIsPhysicalPrimaryPositive = kFALSE;
        fTreeCascVarIsPhysicalPrimaryBachelor = kFALSE;
        fTreeCascVarIsPhysicalPrimaryNegativeMother = kFALSE;
        fTreeCascVarIsPhysicalPrimaryPositiveMother = kFALSE;
        fTreeCascVarIsPhysicalPrimaryBachelorMother = kFALSE;
        fTreeCascVarIsPhysicalPrimaryNegativeGrandMother = kFALSE;
        fTreeCascVarIsPhysicalPrimaryPositiveGrandMother = kFALSE;
        fTreeCascVarIsPhysicalPrimaryBachelorGrandMother = kFALSE;
        
        if( lMCstack->IsPhysicalPrimary( lblNegV0Dghter ) ) fTreeCascVarIsPhysicalPrimaryNegative = kTRUE;
        if( lMCstack->IsPhysicalPrimary( lblPosV0Dghter ) ) fTreeCascVarIsPhysicalPrimaryPositive = kTRUE;
        if( lMCstack->IsPhysicalPrimary( lblBach        ) ) fTreeCascVarIsPhysicalPrimaryBachelor = kTRUE;
        
        fTreeCascVarPosTransvMomentumMC = mcPosV0Dghter->Pt();
        fTreeCascVarNegTransvMomentumMC = mcNegV0Dghter->Pt();
        
        fTreeCascVarPIDPositive = mcPosV0Dghter -> GetPdgCode();
        fTreeCascVarPIDNegative = mcNegV0Dghter -> GetPdgCode();
        fTreeCascVarPIDBachelor = mcBach->GetPdgCode();
        
        // - Step 4.2 : level of the Xi daughters
        
        Int_t lblMotherPosV0Dghter = mcPosV0Dghter->GetFirstMother();
        Int_t lblMotherNegV0Dghter = mcNegV0Dghter->GetFirstMother();
        Int_t lblMotherBachelor    = mcBach->GetFirstMother();
        
        // Extra: check mother particle pdg code completely independently
        // Meant to provide extra info related to the bump
        if ( lblMotherPosV0Dghter > -1 ){
            TParticle *lPosMother = lMCstack->Particle( lblMotherPosV0Dghter );
            if( lMCstack->IsPhysicalPrimary( lblMotherPosV0Dghter ) ) fTreeCascVarIsPhysicalPrimaryPositiveMother = kTRUE;
            fTreeCascVarPIDPositiveMother = lPosMother->GetPdgCode();
            fTreeCascVarPosLabelMother = lblMotherPosV0Dghter;
            //Go further than that, please
            Int_t lblGrandMother = lPosMother->GetFirstMother();
            if( lblGrandMother > -1 ){
                TParticle *lPosGrandMother = lMCstack->Particle( lblGrandMother );
                if( lMCstack->IsPhysicalPrimary( lblGrandMother ) ) fTreeCascVarIsPhysicalPrimaryPositiveGrandMother = kTRUE;
                fTreeCascVarPIDPositiveGrandMother = lPosGrandMother->GetPdgCode();
                fTreeCascVarPosLabelGrandMother = lblGrandMother;
            }
        }
        
        if ( lblMotherNegV0Dghter > -1 ){
            TParticle *lNegMother = lMCstack->Particle( lblMotherNegV0Dghter );
            if( lMCstack->IsPhysicalPrimary( lblMotherNegV0Dghter ) ) fTreeCascVarIsPhysicalPrimaryNegativeMother = kTRUE;
            fTreeCascVarPIDNegativeMother = lNegMother->GetPdgCode();
            fTreeCascVarNegLabelMother = lblMotherNegV0Dghter;
            //Go further than that, please
            Int_t lblGrandMother = lNegMother->GetFirstMother();
            if( lblGrandMother > -1 ){
                TParticle *lNegGrandMother = lMCstack->Particle( lblGrandMother );
                if( lMCstack->IsPhysicalPrimary( lblGrandMother ) ) fTreeCascVarIsPhysicalPrimaryNegativeGrandMother = kTRUE;
                fTreeCascVarPIDNegativeGrandMother = lNegGrandMother->GetPdgCode();
                fTreeCascVarNegLabelGrandMother = lblGrandMother;
            }
        }
        
        if ( lblMotherBachelor > -1 ){
            TParticle *lBachMother = lMCstack->Particle( lblMotherBachelor );
            if( lMCstack->IsPhysicalPrimary( lblMotherBachelor ) ) fTreeCascVarIsPhysicalPrimaryBachelorMother = kTRUE;
            fTreeCascVarPIDBachelorMother = lBachMother->GetPdgCode();
            fTreeCascVarBachLabelMother = lblMotherBachelor;
            //Go further than that, please
            Int_t lblGrandMother = lBachMother->GetFirstMother();
            if( lblGrandMother > -1 ){
                TParticle *lBachGrandMother = lMCstack->Particle( lblGrandMother );
                if( lMCstack->IsPhysicalPrimary( lblGrandMother ) ) fTreeCascVarIsPhysicalPrimaryBachelorGrandMother = kTRUE;
                fTreeCascVarPIDBachelorGrandMother = lBachGrandMother->GetPdgCode();
                fTreeCascVarBachLabelGrandMother = lblGrandMother;
            }
        }
        
        //Rather uncivilized: Open brackets for each 'continue'
        if(! (lblMotherPosV0Dghter != lblMotherNegV0Dghter) ) { // same mother
            if(! (lblMotherPosV0Dghter < 0) ) { // mother != primary (!= -1)
                if(! (lblMotherNegV0Dghter < 0) ) {
                    
                    // mothers = Lambda candidate ... a priori
                    
                    TParticle* mcMotherPosV0Dghter = lMCstack->Particle( lblMotherPosV0Dghter );
                    TParticle* mcMotherNegV0Dghter = lMCstack->Particle( lblMotherNegV0Dghter );
                    
                    // - Step 4.3 : level of Xi candidate
                    //Be careful: Vx, Vy, Vz: Creation vertex. So decay position is the
                    //Creation vertex of any one of the daughters!
                    fTreeCascVarV0DecayXMC = mcPosV0Dghter->Vx();
                    fTreeCascVarV0DecayYMC = mcPosV0Dghter->Vy();
                    fTreeCascVarV0DecayZMC = mcPosV0Dghter->Vz();
                    
                    Int_t lblGdMotherPosV0Dghter =   mcMotherPosV0Dghter->GetFirstMother() ;
                    Int_t lblGdMotherNegV0Dghter =   mcMotherNegV0Dghter->GetFirstMother() ;
                    
                    if(! (lblGdMotherPosV0Dghter != lblGdMotherNegV0Dghter) ) {
                        if(! (lblGdMotherPosV0Dghter < 0) ) { // primary lambda ...
                            if(! (lblGdMotherNegV0Dghter < 0) ) { // primary lambda ...
                                
                                // Gd mothers = Xi candidate ... a priori
                                
                                TParticle* mcGdMotherPosV0Dghter = lMCstack->Particle( lblGdMotherPosV0Dghter );
                                TParticle* mcGdMotherNegV0Dghter = lMCstack->Particle( lblGdMotherNegV0Dghter );
                                
                                Int_t lblMotherBach = (Int_t) TMath::Abs( mcBach->GetFirstMother()  );
                                
                                //		if( lblMotherBach != lblGdMotherPosV0Dghter ) continue; //same mother for bach and V0 daughters
                                if(!(lblMotherBach != lblGdMotherPosV0Dghter)) { //same mother for bach and V0 daughters
                                    
                                    TParticle* mcMotherBach = lMCstack->Particle( lblMotherBach );
                                    
                                    lPID_BachMother = mcMotherBach->GetPdgCode();
                                    lPID_NegMother = mcGdMotherPosV0Dghter->GetPdgCode();
                                    lPID_PosMother = mcGdMotherNegV0Dghter->GetPdgCode();
                                    
                                    if(lPID_BachMother==lPID_NegMother && lPID_BachMother==lPID_PosMother) {
                                        lPDGCodeCascade = lPID_BachMother;
                                        lXiTransvMomMC = mcMotherBach->Pt();
                                        //Be careful: Vx, Vy, Vz: Creation vertex. So decay position is the
                                        //Creation vertex of any one of the daughters!
                                        fTreeCascVarCascadeDecayXMC = mcBach->Vx();
                                        fTreeCascVarCascadeDecayYMC = mcBach->Vy();
                                        fTreeCascVarCascadeDecayZMC = mcBach->Vz();
                                        if( lMCstack->IsPhysicalPrimary       (lblMotherBach) ) fTreeCascVarIsPhysicalPrimary = 1; //Is Primary!
                                        if( lMCstack->IsSecondaryFromWeakDecay(lblMotherBach) ) fTreeCascVarIsPhysicalPrimary = 2; //Weak Decay!
                                        if( lMCstack->IsSecondaryFromMaterial (lblMotherBach) ) fTreeCascVarIsPhysicalPrimary = 3; //From Material!
                                        if ( (mcMotherBach->Energy() + mcMotherBach->Pz()) / (mcMotherBach->Energy() - mcMotherBach->Pz() +1.e-13) !=0 ) {
                                            lRapMC = 0.5*TMath::Log( (mcMotherBach->Energy() + mcMotherBach->Pz()) / (mcMotherBach->Energy() - mcMotherBach->Pz() +1.e-13) );
                                        }
                                    }
                                    
                                }
                            }
                        }
                    }
                }
            }
        } //Ends all conditionals above...
        
        fTreeCascVarV0BachSibIsValid = -1;
        fTreeCascVarV0NegSibIsValid = -1;
        fTreeCascVarV0PosSibIsValid = -1;
        
        //------------------------------------------------
        // Testing V0 association veto for mesons
        //------------------------------------------------
        //Variable definition
        Int_t    lBachSibOnFlyStatus = 0;// nv0sOn = 0, nv0sOff = 0;
        Double_t lBachSibChi2V0 = 0;
        Double_t lBachSibDcaV0Daughters = 0, lBachSibDcaV0ToPrimVertex = 0;
        Double_t lBachSibDcaPosToPrimVertex = 0, lBachSibDcaNegToPrimVertex = 0;
        Double_t lBachSibV0CosineOfPointingAngle = 0;
        Double_t lBachSibV0Radius = 0, lBachSibPt = 0;
        Double_t lBachSibRapK0Short = 0, lBachSibRapLambda = 0;
        Double_t lBachSibInvMassK0s = 0, lBachSibInvMassLambda = 0, lBachSibInvMassAntiLambda = 0;
        Double_t lBachSibAlphaV0 = 0, lBachSibPtArmV0 = 0;
        
        //Neg variables
        Int_t    lNegSibOnFlyStatus = 0;// nv0sOn = 0, nv0sOff = 0;
        Double_t lNegSibChi2V0 = 0;
        Double_t lNegSibDcaV0Daughters = 0, lNegSibDcaV0ToPrimVertex = 0;
        Double_t lNegSibDcaPosToPrimVertex = 0, lNegSibDcaNegToPrimVertex = 0;
        Double_t lNegSibV0CosineOfPointingAngle = 0;
        Double_t lNegSibV0Radius = 0, lNegSibPt = 0;
        Double_t lNegSibRapK0Short = 0, lNegSibRapLambda = 0;
        Double_t lNegSibInvMassK0s = 0, lNegSibInvMassLambda = 0, lNegSibInvMassAntiLambda = 0;
        Double_t lNegSibAlphaV0 = 0, lNegSibPtArmV0 = 0;
        
        //Pos variables
        Int_t    lPosSibOnFlyStatus = 0;// nv0sOn = 0, nv0sOff = 0;
        Double_t lPosSibChi2V0 = 0;
        Double_t lPosSibDcaV0Daughters = 0, lPosSibDcaV0ToPrimVertex = 0;
        Double_t lPosSibDcaPosToPrimVertex = 0, lPosSibDcaNegToPrimVertex = 0;
        Double_t lPosSibV0CosineOfPointingAngle = 0;
        Double_t lPosSibV0Radius = 0, lPosSibPt = 0;
        Double_t lPosSibRapK0Short = 0, lPosSibRapLambda = 0;
        Double_t lPosSibInvMassK0s = 0, lPosSibInvMassLambda = 0, lPosSibInvMassAntiLambda = 0;
        Double_t lPosSibAlphaV0 = 0, lPosSibPtArmV0 = 0;
        
        Double_t fMinV0Pt = 0;
        
        if( TMath::Abs(fTreeCascVarPIDBachelorMother) == 310 ){
            Int_t lBachSibV0Status = 0;
            fTreeCascVarV0BachSibIsValid = 0;
            
            //Need to loop on the V0's
            for (Int_t lV0 = 0; lV0 < nv0s; lV0++){
                AliESDv0 *BachSibv0 = ((AliESDEvent*)lESDevent)->GetV0(lV0);
                if (!BachSibv0) continue;
                
                CheckChargeV0( BachSibv0 );
                //Remove like-sign (will not affect offline V0 candidates!)
                if( BachSibv0->GetParamN()->Charge() > 0 && BachSibv0->GetParamP()->Charge() > 0 ){
                    continue;
                }
                if( BachSibv0->GetParamN()->Charge() < 0 && BachSibv0->GetParamP()->Charge() < 0 ){
                    continue;
                }
                
                UInt_t lBachSibKeyPos = (UInt_t)TMath::Abs(BachSibv0->GetPindex());
                UInt_t lBachSibKeyNeg = (UInt_t)TMath::Abs(BachSibv0->GetNindex());
                
                
                AliESDtrack *BachSibpTrack=((AliESDEvent*)lESDevent)->GetTrack(lBachSibKeyPos);
                AliESDtrack *BachSibnTrack=((AliESDEvent*)lESDevent)->GetTrack(lBachSibKeyNeg);
                if (!BachSibpTrack || !BachSibnTrack) {
                    Printf("ERROR: Could not retreive one of the daughter track");
                    continue;
                }
                Int_t lblBachSibV0P = (Int_t) TMath::Abs( BachSibpTrack->GetLabel() );
                Int_t lblBachSibV0N = (Int_t) TMath::Abs( BachSibnTrack->GetLabel() );
                //Testing if Bachelor belong to this V0
                Int_t lblBachSib = -9999;
                AliESDtrack *BachSibTrack = 0x0;
                if( lblBach == lblBachSibV0P ){
                    lblBachSib = lblBachSibV0N;
                }
                if( lblBach == lblBachSibV0N ){
                    lblBachSib = lblBachSibV0P;
                }
                if( lblBachSib == -9999 ) continue; //Did not find Bachelor in V0
                //BachSibTrack = ((AliESDEvent*)lESDevent)->GetTrack(lblBachSib);
                //Now need to check if sibling comes from same mother as bachellr (K0s)
                
                TParticle* mcBachSib = lMCstack->Particle(lblBachSib);
                
                Int_t lblBachSibMother = mcBachSib->GetFirstMother();
                if( lblBachSibMother != lblMotherBachelor ) continue;//Different mothers
                //Start of track quality checks
                //Now let's test track quality
                const AliExternalTrackParam *BachSibinnernegv0=BachSibnTrack->GetInnerParam();
                const AliExternalTrackParam *BachSibinnerposv0=BachSibpTrack->GetInnerParam();
                Float_t lBachSibPosInnerP = -1;
                Float_t lBachSibNegInnerP = -1;
                if(BachSibinnerposv0)  { lBachSibPosInnerP  = BachSibinnerposv0 ->GetP(); }
                if(BachSibinnernegv0)  { lBachSibNegInnerP  = BachSibinnernegv0 ->GetP(); }
                
                //Daughter Eta for Eta selection, afterwards
                Float_t lBachSibV0NegEta = BachSibnTrack->Eta();
                Float_t lBachSibV0PosEta = BachSibpTrack->Eta();
                
                //Let's not use this for now...
                //if ( fkExtraCleanup ){
                //    if( TMath::Abs( nTrack->Eta() )>0.8 || TMath::Abs( pTrack->Eta() )>0.8 ) continue;
                //}
                
                
                //________________________________________________________________________
                // Track quality cuts
                Float_t lBachSibPosTrackCrossedRows = BachSibpTrack->GetTPCClusterInfo(2,1);
                Float_t lBachSibNegTrackCrossedRows = BachSibnTrack->GetTPCClusterInfo(2,1);
                Int_t lSibV0LeastNbrCrossedRows = (Int_t) lBachSibPosTrackCrossedRows;
                if( lBachSibNegTrackCrossedRows < lSibV0LeastNbrCrossedRows )
                    lSibV0LeastNbrCrossedRows = (Int_t) lBachSibNegTrackCrossedRows;
                
                // TPC refit condition (done during reconstruction for Offline but not for On-the-fly)
                if( !(BachSibpTrack->GetStatus() & AliESDtrack::kTPCrefit)) continue;
                if( !(BachSibnTrack->GetStatus() & AliESDtrack::kTPCrefit)) continue;
                
                //Get status flags
                ULong_t lSibV0PosTrackStatus = BachSibpTrack->GetStatus();
                ULong_t lSibV0NegTrackStatus = BachSibnTrack->GetStatus();
                
                //Compute ratio Crossed Rows / Findable clusters
                //Note: above test avoids division by zero!
                Float_t lBachSibPosTrackCrossedRowsOverFindable = lBachSibPosTrackCrossedRows / ((double)(BachSibpTrack->GetTPCNclsF()));
                Float_t lBachSibNegTrackCrossedRowsOverFindable = lBachSibNegTrackCrossedRows / ((double)(BachSibnTrack->GetTPCNclsF()));
                
                Float_t lSibV0LeastRatioCrossedRowsOverFindable = lBachSibPosTrackCrossedRowsOverFindable;
                if( lBachSibNegTrackCrossedRowsOverFindable < lSibV0LeastRatioCrossedRowsOverFindable )
                    lSibV0LeastRatioCrossedRowsOverFindable = lBachSibNegTrackCrossedRowsOverFindable;
                
                //Lowest Cut Level for Ratio Crossed Rows / Findable = 0.8, set here
                if ( lSibV0LeastRatioCrossedRowsOverFindable < 0.8 ) continue;
                
                
                //Extra track quality: min track length
                Float_t lBachSibSmallestTrackLength = 1000;
                Float_t lBachSibPosTrackLength = -1;
                Float_t lBachSibNegTrackLength = -1;
                
                if (BachSibpTrack->GetInnerParam()) lBachSibPosTrackLength = BachSibpTrack->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
                if (BachSibnTrack->GetInnerParam()) lBachSibNegTrackLength = BachSibnTrack->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
                
                if ( lBachSibPosTrackLength  < lBachSibSmallestTrackLength ) lBachSibSmallestTrackLength = lBachSibPosTrackLength;
                if ( lBachSibNegTrackLength  < lBachSibSmallestTrackLength ) lBachSibSmallestTrackLength = lBachSibNegTrackLength;
                
                //fTreeVariableMinTrackLength = lSmallestTrackLength;
                
                if ( ( ( ( BachSibpTrack->GetTPCClusterInfo(2,1) ) < 70 ) || ( ( BachSibnTrack->GetTPCClusterInfo(2,1) ) < 70 ) ) && lBachSibSmallestTrackLength<80 ) continue;
                
                fTreeCascVarV0BachSibIsValid = 1;
                //End track Quality Cuts
                //Now would be a good time to get V0 informations
                
                Double_t tBachSibDecayVertexV0[3];
                BachSibv0->GetXYZ(tBachSibDecayVertexV0[0],tBachSibDecayVertexV0[1],tBachSibDecayVertexV0[2]);
                
                lBachSibV0Radius = TMath::Sqrt(tBachSibDecayVertexV0[0]*tBachSibDecayVertexV0[0]+tBachSibDecayVertexV0[1]*tBachSibDecayVertexV0[1]);
                
                lBachSibDcaPosToPrimVertex = TMath::Abs(BachSibpTrack->GetD(lBestPrimaryVtxPos[0],
                                                                            lBestPrimaryVtxPos[1],
                                                                            lMagneticField) );
                
                lBachSibDcaNegToPrimVertex = TMath::Abs(BachSibnTrack->GetD(lBestPrimaryVtxPos[0],
                                                                            lBestPrimaryVtxPos[1],
                                                                            lMagneticField) );
                
                lBachSibOnFlyStatus = BachSibv0->GetOnFlyStatus();
                lBachSibChi2V0 = BachSibv0->GetChi2V0();
                lBachSibDcaV0Daughters = BachSibv0->GetDcaV0Daughters();
                lBachSibDcaV0ToPrimVertex = BachSibv0->GetD(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2]);
                lBachSibV0CosineOfPointingAngle = BachSibv0->GetV0CosineOfPointingAngle(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2]);
                BachSibv0->ChangeMassHypothesis(310);
                lBachSibInvMassK0s = BachSibv0->GetEffMass();
                BachSibv0->ChangeMassHypothesis(3122);
                lBachSibInvMassLambda = BachSibv0->GetEffMass();
                BachSibv0->ChangeMassHypothesis(-3122);
                lBachSibInvMassAntiLambda = BachSibv0->GetEffMass();
                lBachSibAlphaV0 = BachSibv0->AlphaV0();
                lBachSibPtArmV0 = BachSibv0->PtArmV0();
                
                fTreeCascVarBachSibPt                      = BachSibv0->Pt();
                fTreeCascVarBachSibDcaV0ToPrimVertex       = lBachSibDcaV0ToPrimVertex;
                fTreeCascVarBachSibDcaV0Daughters          = lBachSibDcaV0Daughters;
                fTreeCascVarBachSibV0CosineOfPointingAngle = lBachSibV0CosineOfPointingAngle;
                fTreeCascVarBachSibV0V0Radius              = lBachSibV0Radius;
                fTreeCascVarBachSibV0DcaPosToPrimVertex    = lBachSibDcaPosToPrimVertex;
                fTreeCascVarBachSibV0DcaNegToPrimVertex    = lBachSibDcaNegToPrimVertex;
                fTreeCascVarBachSibV0InvMassK0s            = lBachSibInvMassK0s;
                fTreeCascVarBachSibV0InvMassLambda         = lBachSibInvMassLambda;
                fTreeCascVarBachSibV0InvMassAntiLambda     = lBachSibInvMassAntiLambda;
                //^^^^^^^^^^^^^^^^^^^^^^
            }
        }
        
        if( TMath::Abs(fTreeCascVarPIDNegativeMother) == 310 ){
            Int_t lNegSibV0Status = 0;
            fTreeCascVarV0NegSibIsValid = 0;
            
            //Need to loop on the V0's
            for (Int_t lV0 = 0; lV0 < nv0s; lV0++){
                AliESDv0 *NegSibv0 = ((AliESDEvent*)lESDevent)->GetV0(lV0);
                if (!NegSibv0) continue;
                
                CheckChargeV0( NegSibv0 );
                //Remove like-sign (will not affect offline V0 candidates!)
                if( NegSibv0->GetParamN()->Charge() > 0 && NegSibv0->GetParamP()->Charge() > 0 ){
                    continue;
                }
                if( NegSibv0->GetParamN()->Charge() < 0 && NegSibv0->GetParamP()->Charge() < 0 ){
                    continue;
                }
                
                UInt_t lNegSibKeyPos = (UInt_t)TMath::Abs(NegSibv0->GetPindex());
                UInt_t lNegSibKeyNeg = (UInt_t)TMath::Abs(NegSibv0->GetNindex());
                
                
                AliESDtrack *NegSibpTrack=((AliESDEvent*)lESDevent)->GetTrack(lNegSibKeyPos);
                AliESDtrack *NegSibnTrack=((AliESDEvent*)lESDevent)->GetTrack(lNegSibKeyNeg);
                if (!NegSibpTrack || !NegSibnTrack) {
                    Printf("ERROR: Could not retreive one of the daughter track");
                    continue;
                }
                Int_t lblNegSibV0P = (Int_t) TMath::Abs( NegSibpTrack->GetLabel() );
                Int_t lblNegSibV0N = (Int_t) TMath::Abs( NegSibnTrack->GetLabel() );
                //Testing if Negative belong to this V0
                Int_t lblNegSib = -9999;
                AliESDtrack *NegSibTrack = 0x0;
                if( lblNegV0Dghter == lblNegSibV0P ){
                    lblNegSib = lblNegSibV0N;
                }
                if( lblNegV0Dghter == lblNegSibV0N ){
                    lblNegSib = lblNegSibV0P;
                }
                if( lblNegSib == -9999 ) continue; //Did not find Negative in V0
                //NegSibTrack = ((AliESDEvent*)lESDevent)->GetTrack(lblNegSib);
                //Now need to check if sibling comes from same mother as bachellr (K0s)
                
                TParticle* mcNegSib = lMCstack->Particle(lblNegSib);
                
                Int_t lblNegSibMother = mcNegSib->GetFirstMother();
                if( lblNegSibMother != lblMotherNegV0Dghter ) continue;//Different mothers
                //Start of track quality checks
                //Now let's test track quality
                const AliExternalTrackParam *NegSibinnernegv0=NegSibnTrack->GetInnerParam();
                const AliExternalTrackParam *NegSibinnerposv0=NegSibpTrack->GetInnerParam();
                Float_t lNegSibPosInnerP = -1;
                Float_t lNegSibNegInnerP = -1;
                if(NegSibinnerposv0)  { lNegSibPosInnerP  = NegSibinnerposv0 ->GetP(); }
                if(NegSibinnernegv0)  { lNegSibNegInnerP  = NegSibinnernegv0 ->GetP(); }
                
                //Daughter Eta for Eta selection, afterwards
                Float_t lNegSibV0NegEta = NegSibnTrack->Eta();
                Float_t lNegSibV0PosEta = NegSibpTrack->Eta();
                
                //Let's not use this for now...
                //if ( fkExtraCleanup ){
                //    if( TMath::Abs( nTrack->Eta() )>0.8 || TMath::Abs( pTrack->Eta() )>0.8 ) continue;
                //}
                
                
                //________________________________________________________________________
                // Track quality cuts
                Float_t lNegSibPosTrackCrossedRows = NegSibpTrack->GetTPCClusterInfo(2,1);
                Float_t lNegSibNegTrackCrossedRows = NegSibnTrack->GetTPCClusterInfo(2,1);
                Int_t lSibV0LeastNbrCrossedRows = (Int_t) lNegSibPosTrackCrossedRows;
                if( lNegSibNegTrackCrossedRows < lSibV0LeastNbrCrossedRows )
                    lSibV0LeastNbrCrossedRows = (Int_t) lNegSibNegTrackCrossedRows;
                
                // TPC refit condition (done during reconstruction for Offline but not for On-the-fly)
                if( !(NegSibpTrack->GetStatus() & AliESDtrack::kTPCrefit)) continue;
                if( !(NegSibnTrack->GetStatus() & AliESDtrack::kTPCrefit)) continue;
                
                //Get status flags
                ULong_t lSibV0PosTrackStatus = NegSibpTrack->GetStatus();
                ULong_t lSibV0NegTrackStatus = NegSibnTrack->GetStatus();
                
                //Compute ratio Crossed Rows / Findable clusters
                //Note: above test avoids division by zero!
                Float_t lNegSibPosTrackCrossedRowsOverFindable = lNegSibPosTrackCrossedRows / ((double)(NegSibpTrack->GetTPCNclsF()));
                Float_t lNegSibNegTrackCrossedRowsOverFindable = lNegSibNegTrackCrossedRows / ((double)(NegSibnTrack->GetTPCNclsF()));
                
                Float_t lSibV0LeastRatioCrossedRowsOverFindable = lNegSibPosTrackCrossedRowsOverFindable;
                if( lNegSibNegTrackCrossedRowsOverFindable < lSibV0LeastRatioCrossedRowsOverFindable )
                    lSibV0LeastRatioCrossedRowsOverFindable = lNegSibNegTrackCrossedRowsOverFindable;
                
                //Lowest Cut Level for Ratio Crossed Rows / Findable = 0.8, set here
                if ( lSibV0LeastRatioCrossedRowsOverFindable < 0.8 ) continue;
                
                
                //Extra track quality: min track length
                Float_t lNegSibSmallestTrackLength = 1000;
                Float_t lNegSibPosTrackLength = -1;
                Float_t lNegSibNegTrackLength = -1;
                
                if (NegSibpTrack->GetInnerParam()) lNegSibPosTrackLength = NegSibpTrack->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
                if (NegSibnTrack->GetInnerParam()) lNegSibNegTrackLength = NegSibnTrack->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
                
                if ( lNegSibPosTrackLength  < lNegSibSmallestTrackLength ) lNegSibSmallestTrackLength = lNegSibPosTrackLength;
                if ( lNegSibNegTrackLength  < lNegSibSmallestTrackLength ) lNegSibSmallestTrackLength = lNegSibNegTrackLength;
                
                //fTreeVariableMinTrackLength = lSmallestTrackLength;
                
                if ( ( ( ( NegSibpTrack->GetTPCClusterInfo(2,1) ) < 70 ) || ( ( NegSibnTrack->GetTPCClusterInfo(2,1) ) < 70 ) ) && lNegSibSmallestTrackLength<80 ) continue;
                
                fTreeCascVarV0NegSibIsValid = 1;
                //End track Quality Cuts
                //Now would be a good time to get V0 informations
                
                Double_t tNegSibDecayVertexV0[3];
                NegSibv0->GetXYZ(tNegSibDecayVertexV0[0],tNegSibDecayVertexV0[1],tNegSibDecayVertexV0[2]);
                
                lNegSibV0Radius = TMath::Sqrt(tNegSibDecayVertexV0[0]*tNegSibDecayVertexV0[0]+tNegSibDecayVertexV0[1]*tNegSibDecayVertexV0[1]);
                
                lNegSibDcaPosToPrimVertex = TMath::Abs(NegSibpTrack->GetD(lBestPrimaryVtxPos[0],
                                                                            lBestPrimaryVtxPos[1],
                                                                            lMagneticField) );
                
                lNegSibDcaNegToPrimVertex = TMath::Abs(NegSibnTrack->GetD(lBestPrimaryVtxPos[0],
                                                                            lBestPrimaryVtxPos[1],
                                                                            lMagneticField) );
                
                lNegSibOnFlyStatus = NegSibv0->GetOnFlyStatus();
                lNegSibChi2V0 = NegSibv0->GetChi2V0();
                lNegSibDcaV0Daughters = NegSibv0->GetDcaV0Daughters();
                lNegSibDcaV0ToPrimVertex = NegSibv0->GetD(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2]);
                lNegSibV0CosineOfPointingAngle = NegSibv0->GetV0CosineOfPointingAngle(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2]);
                NegSibv0->ChangeMassHypothesis(310);
                lNegSibInvMassK0s = NegSibv0->GetEffMass();
                NegSibv0->ChangeMassHypothesis(3122);
                lNegSibInvMassLambda = NegSibv0->GetEffMass();
                NegSibv0->ChangeMassHypothesis(-3122);
                lNegSibInvMassAntiLambda = NegSibv0->GetEffMass();
                lNegSibAlphaV0 = NegSibv0->AlphaV0();
                lNegSibPtArmV0 = NegSibv0->PtArmV0();
                
                
                fTreeCascVarNegSibPt                      = NegSibv0->Pt();
                fTreeCascVarNegSibDcaV0ToPrimVertex       = lNegSibDcaV0ToPrimVertex;
                fTreeCascVarNegSibDcaV0Daughters          = lNegSibDcaV0Daughters;
                fTreeCascVarNegSibV0CosineOfPointingAngle = lNegSibV0CosineOfPointingAngle;
                fTreeCascVarNegSibV0V0Radius              = lNegSibV0Radius;
                fTreeCascVarNegSibV0DcaPosToPrimVertex    = lNegSibDcaPosToPrimVertex;
                fTreeCascVarNegSibV0DcaNegToPrimVertex    = lNegSibDcaNegToPrimVertex;
                fTreeCascVarNegSibV0InvMassK0s            = lNegSibInvMassK0s;
                fTreeCascVarNegSibV0InvMassLambda         = lNegSibInvMassLambda;
                fTreeCascVarNegSibV0InvMassAntiLambda     = lNegSibInvMassAntiLambda;
                //^^^^^^^^^^^^^^^^^^^^^^
            }
        }

        if( TMath::Abs(fTreeCascVarPIDPositiveMother) == 310 ){
            Int_t lPosSibV0Status = 0;
            fTreeCascVarV0PosSibIsValid = 0;
            
            //Need to loop on the V0's
            for (Int_t lV0 = 0; lV0 < nv0s; lV0++){
                AliESDv0 *PosSibv0 = ((AliESDEvent*)lESDevent)->GetV0(lV0);
                if (!PosSibv0) continue;
                
                CheckChargeV0( PosSibv0 );
                //Remove like-sign (will not affect offline V0 candidates!)
                if( PosSibv0->GetParamN()->Charge() > 0 && PosSibv0->GetParamP()->Charge() > 0 ){
                    continue;
                }
                if( PosSibv0->GetParamN()->Charge() < 0 && PosSibv0->GetParamP()->Charge() < 0 ){
                    continue;
                }
                
                UInt_t lPosSibKeyPos = (UInt_t)TMath::Abs(PosSibv0->GetPindex());
                UInt_t lPosSibKeyNeg = (UInt_t)TMath::Abs(PosSibv0->GetNindex());
                
                
                AliESDtrack *PosSibpTrack=((AliESDEvent*)lESDevent)->GetTrack(lPosSibKeyPos);
                AliESDtrack *PosSibnTrack=((AliESDEvent*)lESDevent)->GetTrack(lPosSibKeyNeg);
                if (!PosSibpTrack || !PosSibnTrack) {
                    Printf("ERROR: Could not retreive one of the daughter track");
                    continue;
                }
                Int_t lblPosSibV0P = (Int_t) TMath::Abs( PosSibpTrack->GetLabel() );
                Int_t lblPosSibV0N = (Int_t) TMath::Abs( PosSibnTrack->GetLabel() );
                //Testing if Positive belong to this V0
                Int_t lblPosSib = -9999;
                AliESDtrack *PosSibTrack = 0x0;
                if( lblPosV0Dghter == lblPosSibV0P ){
                    lblPosSib = lblPosSibV0N;
                }
                if( lblPosV0Dghter == lblPosSibV0N ){
                    lblPosSib = lblPosSibV0P;
                }
                if( lblPosSib == -9999 ) continue; //Did not find Positive in V0
                //PosSibTrack = ((AliESDEvent*)lESDevent)->GetTrack(lblPosSib);
                //Now need to check if sibling comes from same mother as bachellr (K0s)
                
                TParticle* mcPosSib = lMCstack->Particle(lblPosSib);
                
                Int_t lblPosSibMother = mcPosSib->GetFirstMother();
                if( lblPosSibMother != lblMotherPosV0Dghter ) continue;//Different mothers
                //Start of track quality checks
                //Now let's test track quality
                const AliExternalTrackParam *PosSibinnernegv0=PosSibnTrack->GetInnerParam();
                const AliExternalTrackParam *PosSibinnerposv0=PosSibpTrack->GetInnerParam();
                Float_t lPosSibPosInnerP = -1;
                Float_t lPosSibNegInnerP = -1;
                if(PosSibinnerposv0)  { lPosSibPosInnerP  = PosSibinnerposv0 ->GetP(); }
                if(PosSibinnernegv0)  { lPosSibNegInnerP  = PosSibinnernegv0 ->GetP(); }
                
                //Daughter Eta for Eta selection, afterwards
                Float_t lPosSibV0NegEta = PosSibnTrack->Eta();
                Float_t lPosSibV0PosEta = PosSibpTrack->Eta();
                
                //Let's not use this for now...
                //if ( fkExtraCleanup ){
                //    if( TMath::Abs( nTrack->Eta() )>0.8 || TMath::Abs( pTrack->Eta() )>0.8 ) continue;
                //}
                
                
                //________________________________________________________________________
                // Track quality cuts
                Float_t lPosSibPosTrackCrossedRows = PosSibpTrack->GetTPCClusterInfo(2,1);
                Float_t lPosSibNegTrackCrossedRows = PosSibnTrack->GetTPCClusterInfo(2,1);
                Int_t lSibV0LeastNbrCrossedRows = (Int_t) lPosSibPosTrackCrossedRows;
                if( lPosSibNegTrackCrossedRows < lSibV0LeastNbrCrossedRows )
                    lSibV0LeastNbrCrossedRows = (Int_t) lPosSibNegTrackCrossedRows;
                
                // TPC refit condition (done during reconstruction for Offline but not for On-the-fly)
                if( !(PosSibpTrack->GetStatus() & AliESDtrack::kTPCrefit)) continue;
                if( !(PosSibnTrack->GetStatus() & AliESDtrack::kTPCrefit)) continue;
                
                //Get status flags
                ULong_t lSibV0PosTrackStatus = PosSibpTrack->GetStatus();
                ULong_t lSibV0NegTrackStatus = PosSibnTrack->GetStatus();
                
                //Compute ratio Crossed Rows / Findable clusters
                //Note: above test avoids division by zero!
                Float_t lPosSibPosTrackCrossedRowsOverFindable = lPosSibPosTrackCrossedRows / ((double)(PosSibpTrack->GetTPCNclsF()));
                Float_t lPosSibNegTrackCrossedRowsOverFindable = lPosSibNegTrackCrossedRows / ((double)(PosSibnTrack->GetTPCNclsF()));
                
                Float_t lSibV0LeastRatioCrossedRowsOverFindable = lPosSibPosTrackCrossedRowsOverFindable;
                if( lPosSibNegTrackCrossedRowsOverFindable < lSibV0LeastRatioCrossedRowsOverFindable )
                    lSibV0LeastRatioCrossedRowsOverFindable = lPosSibNegTrackCrossedRowsOverFindable;
                
                //Lowest Cut Level for Ratio Crossed Rows / Findable = 0.8, set here
                if ( lSibV0LeastRatioCrossedRowsOverFindable < 0.8 ) continue;
                
                
                //Extra track quality: min track length
                Float_t lPosSibSmallestTrackLength = 1000;
                Float_t lPosSibPosTrackLength = -1;
                Float_t lPosSibNegTrackLength = -1;
                
                if (PosSibpTrack->GetInnerParam()) lPosSibPosTrackLength = PosSibpTrack->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
                if (PosSibnTrack->GetInnerParam()) lPosSibNegTrackLength = PosSibnTrack->GetLengthInActiveZone(1, 2.0, 220.0, lESDevent->GetMagneticField());
                
                if ( lPosSibPosTrackLength  < lPosSibSmallestTrackLength ) lPosSibSmallestTrackLength = lPosSibPosTrackLength;
                if ( lPosSibNegTrackLength  < lPosSibSmallestTrackLength ) lPosSibSmallestTrackLength = lPosSibNegTrackLength;
                
                //fTreeVariableMinTrackLength = lSmallestTrackLength;
                
                if ( ( ( ( PosSibpTrack->GetTPCClusterInfo(2,1) ) < 70 ) || ( ( PosSibnTrack->GetTPCClusterInfo(2,1) ) < 70 ) ) && lPosSibSmallestTrackLength<80 ) continue;
                
                fTreeCascVarV0PosSibIsValid = 1;
                //End track Quality Cuts
                //Now would be a good time to get V0 informations
                
                Double_t tPosSibDecayVertexV0[3];
                PosSibv0->GetXYZ(tPosSibDecayVertexV0[0],tPosSibDecayVertexV0[1],tPosSibDecayVertexV0[2]);
                
                lPosSibV0Radius = TMath::Sqrt(tPosSibDecayVertexV0[0]*tPosSibDecayVertexV0[0]+tPosSibDecayVertexV0[1]*tPosSibDecayVertexV0[1]);
                
                lPosSibDcaPosToPrimVertex = TMath::Abs(PosSibpTrack->GetD(lBestPrimaryVtxPos[0],
                                                                            lBestPrimaryVtxPos[1],
                                                                            lMagneticField) );
                
                lPosSibDcaNegToPrimVertex = TMath::Abs(PosSibnTrack->GetD(lBestPrimaryVtxPos[0],
                                                                            lBestPrimaryVtxPos[1],
                                                                            lMagneticField) );
                
                lPosSibOnFlyStatus = PosSibv0->GetOnFlyStatus();
                lPosSibChi2V0 = PosSibv0->GetChi2V0();
                lPosSibDcaV0Daughters = PosSibv0->GetDcaV0Daughters();
                lPosSibDcaV0ToPrimVertex = PosSibv0->GetD(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2]);
                lPosSibV0CosineOfPointingAngle = PosSibv0->GetV0CosineOfPointingAngle(lBestPrimaryVtxPos[0],lBestPrimaryVtxPos[1],lBestPrimaryVtxPos[2]);
                PosSibv0->ChangeMassHypothesis(310);
                lPosSibInvMassK0s = PosSibv0->GetEffMass();
                PosSibv0->ChangeMassHypothesis(3122);
                lPosSibInvMassLambda = PosSibv0->GetEffMass();
                PosSibv0->ChangeMassHypothesis(-3122);
                lPosSibInvMassAntiLambda = PosSibv0->GetEffMass();
                lPosSibAlphaV0 = PosSibv0->AlphaV0();
                lPosSibPtArmV0 = PosSibv0->PtArmV0();
                
                fTreeCascVarPosSibPt                      = PosSibv0->Pt();
                fTreeCascVarPosSibDcaV0ToPrimVertex       = lPosSibDcaV0ToPrimVertex;
                fTreeCascVarPosSibDcaV0Daughters          = lPosSibDcaV0Daughters;
                fTreeCascVarPosSibV0CosineOfPointingAngle = lPosSibV0CosineOfPointingAngle;
                fTreeCascVarPosSibV0V0Radius              = lPosSibV0Radius;
                fTreeCascVarPosSibV0DcaPosToPrimVertex    = lPosSibDcaPosToPrimVertex;
                fTreeCascVarPosSibV0DcaNegToPrimVertex    = lPosSibDcaNegToPrimVertex;
                fTreeCascVarPosSibV0InvMassK0s            = lPosSibInvMassK0s;
                fTreeCascVarPosSibV0InvMassLambda         = lPosSibInvMassLambda;
                fTreeCascVarPosSibV0InvMassAntiLambda     = lPosSibInvMassAntiLambda;
                //^^^^^^^^^^^^^^^^^^^^^^
            }
        }

        //----------------------------------------
        // Regular MC ASSOCIATION ENDS HERE
        //----------------------------------------
        
        //----------------------------------------
        // Swapped MC ASSOCIATION STARTS HERE
        // WARNING: THIS IS EXPERIMENTAL!
        //----------------------------------------
        
        // Abs value = needed ! question of quality track association ...
        Int_t lblPosV0DghterSwapped = -1;
        Int_t lblNegV0DghterSwapped = -1;
        Int_t lblBachSwapped = -1;
        
        //Here's where it all happens: please swap stuff around
        //...and "Keep Calm and Carry On"
        if (lChargeXi < 0){
            lblPosV0DghterSwapped = lblPosV0Dghter;
            lblNegV0DghterSwapped = lblBach;
            lblBachSwapped        = lblNegV0Dghter;
        }else{
            lblPosV0DghterSwapped = lblBach;
            lblNegV0DghterSwapped = lblNegV0Dghter;
            lblBachSwapped        = lblPosV0Dghter;
        }
        
        TParticle* mcPosV0DghterSwapped = lMCstack->Particle( lblPosV0DghterSwapped );
        TParticle* mcNegV0DghterSwapped = lMCstack->Particle( lblNegV0DghterSwapped );
        TParticle* mcBachSwapped        = lMCstack->Particle( lblBachSwapped );
        
        //Needed, just for checks
        Int_t lPID_BachMotherSwapped = 0;
        Int_t lPID_NegMotherSwapped = 0;
        Int_t lPID_PosMotherSwapped = 0;
        Int_t lPDGCodeCascadeSwapped = 0;
        
        // - Step 4.2 : level of the Xi daughters
        
        Int_t lblMotherPosV0DghterSwapped = mcPosV0DghterSwapped->GetFirstMother() ;
        Int_t lblMotherNegV0DghterSwapped = mcNegV0DghterSwapped->GetFirstMother();
        
        //Rather uncivilized: Open brackets for each 'continue'
        if(! (lblMotherPosV0DghterSwapped != lblMotherNegV0DghterSwapped) ) { // same mother
            if(! (lblMotherPosV0DghterSwapped < 0) ) { // mother != primary (!= -1)
                if(! (lblMotherNegV0DghterSwapped < 0) ) {
                    
                    // mothers = Lambda candidate ... a priori
                    
                    TParticle* mcMotherPosV0DghterSwapped = lMCstack->Particle( lblMotherPosV0DghterSwapped );
                    TParticle* mcMotherNegV0DghterSwapped = lMCstack->Particle( lblMotherNegV0DghterSwapped );
                    
                    // - Step 4.3 : level of Xi candidate
                    
                    Int_t lblGdMotherPosV0DghterSwapped =   mcMotherPosV0DghterSwapped->GetFirstMother() ;
                    Int_t lblGdMotherNegV0DghterSwapped =   mcMotherNegV0DghterSwapped->GetFirstMother() ;
                    
                    if(! (lblGdMotherPosV0DghterSwapped != lblGdMotherNegV0DghterSwapped) ) {
                        if(! (lblGdMotherPosV0DghterSwapped < 0) ) { // primary lambda ...
                            if(! (lblGdMotherNegV0DghterSwapped < 0) ) { // primary lambda ...
                                
                                // Gd mothers = Xi candidate ... a priori
                                
                                TParticle* mcGdMotherPosV0DghterSwapped = lMCstack->Particle( lblGdMotherPosV0DghterSwapped );
                                TParticle* mcGdMotherNegV0DghterSwapped = lMCstack->Particle( lblGdMotherNegV0DghterSwapped );
                                
                                Int_t lblMotherBachSwapped = (Int_t) TMath::Abs( mcBachSwapped->GetFirstMother()  );
                                
                                //		if( lblMotherBach != lblGdMotherPosV0Dghter ) continue; //same mother for bach and V0 daughters
                                if(!(lblMotherBachSwapped != lblGdMotherPosV0DghterSwapped)) { //same mother for bach and V0 daughters
                                    
                                    TParticle* mcMotherBachSwapped = lMCstack->Particle( lblMotherBachSwapped );
                                    
                                    lPID_BachMotherSwapped = mcMotherBachSwapped->GetPdgCode();
                                    lPID_NegMotherSwapped = mcGdMotherPosV0DghterSwapped->GetPdgCode();
                                    lPID_PosMotherSwapped = mcGdMotherNegV0DghterSwapped->GetPdgCode();
                                    
                                    if(lPID_BachMotherSwapped==lPID_NegMotherSwapped && lPID_BachMotherSwapped==lPID_PosMotherSwapped) {
                                        lPDGCodeCascadeSwapped = lPID_BachMotherSwapped;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        } //Ends all conditionals above...
        
        //----------------------------------------
        // Swapped MC ASSOCIATION ENDS HERE
        //----------------------------------------
        
        
        //------------------------------------------------
        // Set Variables for adding to tree
        //------------------------------------------------
        
        fTreeCascVarCharge	= lChargeXi;
        if (lChargeXi < 0 ){
            fTreeCascVarMassAsXi    = lInvMassXiMinus;
            fTreeCascVarMassAsOmega = lInvMassOmegaMinus;
        }
        if (lChargeXi > 0 ){
            fTreeCascVarMassAsXi    = lInvMassXiPlus;
            fTreeCascVarMassAsOmega = lInvMassOmegaPlus;
        }
        
        fTreeCascVarMVPileupFlag = fMVPileupFlag;
        fTreeCascVarPID = lPDGCodeCascade;
        fTreeCascVarSwappedPID = lPDGCodeCascadeSwapped;
        fTreeCascVarPt = lXiTransvMom;
        fTreeCascVarPtMC = lXiTransvMomMC;
        fTreeCascVarRapXi = lRapXi ;
        fTreeCascVarRapOmega = lRapOmega ;
        fTreeCascVarRapMC = lRapMC ;
        fTreeCascVarDCACascDaughters = lDcaXiDaughters;
        fTreeCascVarDCABachToPrimVtx = lDcaBachToPrimVertexXi;
        fTreeCascVarDCAV0Daughters = lDcaV0DaughtersXi;
        fTreeCascVarDCAV0ToPrimVtx = lDcaV0ToPrimVertexXi;
        fTreeCascVarDCAPosToPrimVtx = lDcaPosToPrimVertexXi;
        fTreeCascVarDCANegToPrimVtx = lDcaNegToPrimVertexXi;
        fTreeCascVarCascCosPointingAngle = lXiCosineOfPointingAngle;
        fTreeCascVarCascRadius = lXiRadius;
        fTreeCascVarV0Mass = lInvMassLambdaAsCascDghter;
        fTreeCascVarV0CosPointingAngle = lV0CosineOfPointingAngleXi;
        fTreeCascVarV0CosPointingAngleSpecial = lV0CosineOfPointingAngleXiSpecial;
        fTreeCascVarV0Radius = lV0RadiusXi;
        fTreeCascVarLeastNbrClusters = leastnumberofclusters;
        fTreeCascVarMaxChi2PerCluster = lBiggestChi2PerCluster;
        
        //Copy Multiplicity information
        fTreeCascVarCentrality = fCentrality;
        
        fTreeCascVarDistOverTotMom = TMath::Sqrt(
                                                 TMath::Power( lPosXi[0] - lBestPrimaryVtxPos[0] , 2) +
                                                 TMath::Power( lPosXi[1] - lBestPrimaryVtxPos[1] , 2) +
                                                 TMath::Power( lPosXi[2] - lBestPrimaryVtxPos[2] , 2)
                                                 );
        fTreeCascVarDistOverTotMom /= (lXiTotMom+1e-13);
        
        //Info for pileup studies
        if( fkDebugOOBPileup ) {
            fTreeCascVarBachTOFExpTDiff = bachTrackXi->GetTOFExpTDiff( bMag );
            fTreeCascVarNegTOFExpTDiff = nTrackXi->GetTOFExpTDiff( bMag );
            fTreeCascVarPosTOFExpTDiff = pTrackXi->GetTOFExpTDiff( bMag );
            fTreeCascVarBachTOFSignal = bachTrackXi->GetTOFsignal() * 1.e-3; // in ns
            fTreeCascVarNegTOFSignal = nTrackXi->GetTOFsignal() * 1.e-3; // in ns
            fTreeCascVarPosTOFSignal = pTrackXi->GetTOFsignal() * 1.e-3; // in ns
            //Copy OOB pileup flag for this event
            fTreeCascVarOOBPileupFlag = fOOBPileupFlag;
            //Copy VZERO information for this event
            fTreeCascVarAmplitudeV0A = fAmplitudeV0A;
            fTreeCascVarAmplitudeV0C = fAmplitudeV0C;
            //Copy FMD information for this event
            fTreeCascVarNHitsFMDA = fNHitsFMDA;
            fTreeCascVarNHitsFMDC = fNHitsFMDC;
        }
        
        if ( fkExtraCleanup ){
            //Meant to provide extra level of cleanup
            if( TMath::Abs(fTreeCascVarPosEta)>0.8 || TMath::Abs(fTreeCascVarNegEta)>0.8 || TMath::Abs(fTreeCascVarBachEta)>0.8 ) continue;
            if( TMath::Abs(fTreeCascVarRapXi)>0.5 && TMath::Abs(fTreeCascVarRapOmega)>0.5 ) continue;
            if ( fkPreselectDedx ){
                Bool_t lPassesPreFilterdEdx = kFALSE;
                //XiMinus Pre-selection
                if( fTreeCascVarMassAsXi<1.32+0.250&&fTreeCascVarMassAsXi>1.32-0.250 && TMath::Abs(fTreeCascVarPosNSigmaProton) < 5.0 && TMath::Abs(fTreeCascVarNegNSigmaPion) < 5.0 && TMath::Abs(fTreeCascVarBachNSigmaPion) < 5.0 && fTreeCascVarCharge == -1 ) lPassesPreFilterdEdx = kTRUE;
                if( fTreeCascVarMassAsXi<1.32+0.250&&fTreeCascVarMassAsXi>1.32-0.250 && TMath::Abs(fTreeCascVarPosNSigmaPion) < 5.0 && TMath::Abs(fTreeCascVarNegNSigmaProton) < 5.0 && TMath::Abs(fTreeCascVarBachNSigmaPion) < 5.0 && fTreeCascVarCharge == +1 ) lPassesPreFilterdEdx = kTRUE;
                if(fTreeCascVarMassAsOmega<1.68+0.250&&fTreeCascVarMassAsOmega>1.68-0.250 && TMath::Abs(fTreeCascVarPosNSigmaProton) < 5.0 && TMath::Abs(fTreeCascVarNegNSigmaPion) < 5.0 && TMath::Abs(fTreeCascVarBachNSigmaKaon) < 5.0 && fTreeCascVarCharge == -1  ) lPassesPreFilterdEdx = kTRUE;
                if(fTreeCascVarMassAsOmega<1.68+0.250&&fTreeCascVarMassAsOmega>1.68-0.250 && TMath::Abs(fTreeCascVarPosNSigmaPion) < 5.0 && TMath::Abs(fTreeCascVarNegNSigmaProton) < 5.0 && TMath::Abs(fTreeCascVarBachNSigmaKaon) < 5.0 && fTreeCascVarCharge == +1) lPassesPreFilterdEdx = kTRUE;
                if( !lPassesPreFilterdEdx ) continue;
            }
        }
        
        //All vars not specified here: specified elsewhere!
        
        //------------------------------------------------
        // Fill Tree!
        //------------------------------------------------
        
        // The conditional is meant to decrease excessive
        // memory usage! Be careful when loosening the
        // cut!
        
        //Xi    Mass window: 150MeV wide
        //Omega mass window: 150MeV wide
        
        //Random denial
        Bool_t lKeepCascade = kTRUE;
        if(fkDownScaleCascade && ( fRand->Uniform() > fDownScaleFactorCascade )) lKeepCascade = kFALSE;
        
        //Lowest pT cutoff (this is all background anyways)
        if( fTreeCascVarPt < fMinPtToSave ) lKeepCascade = kFALSE;
        if( fTreeCascVarPt > fMaxPtToSave ) lKeepCascade = kFALSE;
        
        //Preselect PID: will now select based on daughters only
        if( fkSaveCascadeTree && lKeepCascade && (
                                                  //Xi Conditionals
                                                  (fTreeCascVarMassAsXi<1.32+0.075&&fTreeCascVarMassAsXi>1.32-0.075 &&
                                                   (!fkPreselectPID ||
                                                    TMath::Abs(fTreeCascVarPID)==3312
                                                    )
                                                   ) ||
                                                  //Omega Conditionals
                                                  (fTreeCascVarMassAsOmega<1.68+0.075&&fTreeCascVarMassAsOmega>1.68-0.075 &&
                                                   (!fkPreselectPID ||
                                                    TMath::Abs(fTreeCascVarPID)==3334
                                                    )
                                                   )
                                                  )
           )
        {
            fTreeCascade->Fill();
        }
        //------------------------------------------------
        // Fill tree over.
        //------------------------------------------------
        
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // Superlight adaptive output mode
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
        //Step 1: Sweep members of the output object TList and fill all of them as appropriate
        Int_t lNumberOfConfigurationsCascade = fListCascade->GetEntries();
        //AliWarning(Form("[Cascade Analyses] Processing different configurations (%i detected)",lNumberOfConfigurationsCascade));
        TH3F *histoout         = 0x0;
        TProfile *histoProtonProfile         = 0x0;
        AliCascadeResult *lCascadeResult = 0x0;
        for(Int_t lcfg=0; lcfg<lNumberOfConfigurationsCascade; lcfg++){
            lCascadeResult = (AliCascadeResult*) fListCascade->At(lcfg);
            histoout  = lCascadeResult->GetHistogram();
            histoProtonProfile  = lCascadeResult->GetProtonProfile();
            
            Float_t lMass = 0;
            Float_t lRap  = 0;
            Float_t lPDGMass = -1;
            Float_t lNegdEdx = 100;
            Float_t lPosdEdx = 100;
            Float_t lBachdEdx = 100;
            Short_t  lCharge = -2;
            Int_t lPDGCode = 0;
            Float_t lprpx, lprpy, lprpz, lpipx, lpipy, lpipz;
            lpipx = fTreeCascVarBachPx;
            lpipy = fTreeCascVarBachPy;
            lpipz = fTreeCascVarBachPz;
            Float_t lBaryonTransvMomMCForG3F;
            
            //For parametric V0 Mass selection
            Float_t lExpV0Mass =
            fLambdaMassMean[0]+
            fLambdaMassMean[1]*TMath::Exp(fLambdaMassMean[2]*lV0Pt)+
            fLambdaMassMean[3]*TMath::Exp(fLambdaMassMean[4]*lV0Pt);
            
            Float_t lExpV0Sigma =
            fLambdaMassSigma[0]+fLambdaMassSigma[1]*lV0Pt+
            fLambdaMassSigma[2]*TMath::Exp(fLambdaMassSigma[3]*lV0Pt);
            
            //========================================================================
            //For 2.76TeV-like parametric V0 CosPA
            Float_t l276TeVV0CosPA = 0.998;
            Float_t pThr=1.5;
            if (lV0TotMomentum<pThr) {
                //Below the threshold "pThr", try a momentum dependent cos(PA) cut
                const Double_t bend=0.03; // approximate Xi bending angle
                const Double_t qt=0.211;  // max Lambda pT in Omega decay
                const Double_t cpaThr=TMath::Cos(TMath::ATan(qt/pThr) + bend);
                Double_t
                cpaCut=(0.998/cpaThr)*TMath::Cos(TMath::ATan(qt/lV0TotMomentum) + bend);
                l276TeVV0CosPA = cpaCut;
            }
            //========================================================================
            
            //========================================================================
            //Setting up: Variable Cascade CosPA
            Float_t lCascCosPACut = lCascadeResult -> GetCutCascCosPA();
            Float_t lVarCascCosPApar[5];
            lVarCascCosPApar[0] = lCascadeResult->GetCutVarCascCosPAExp0Const();
            lVarCascCosPApar[1] = lCascadeResult->GetCutVarCascCosPAExp0Slope();
            lVarCascCosPApar[2] = lCascadeResult->GetCutVarCascCosPAExp1Const();
            lVarCascCosPApar[3] = lCascadeResult->GetCutVarCascCosPAExp1Slope();
            lVarCascCosPApar[4] = lCascadeResult->GetCutVarCascCosPAConst();
            Float_t lVarCascCosPA = TMath::Cos(
                                               lVarCascCosPApar[0]*TMath::Exp(lVarCascCosPApar[1]*fTreeCascVarPt) +
                                               lVarCascCosPApar[2]*TMath::Exp(lVarCascCosPApar[3]*fTreeCascVarPt) +
                                               lVarCascCosPApar[4]);
            if( lCascadeResult->GetCutUseVarCascCosPA() ){
                //Only use if tighter than the non-variable cut
                if( lVarCascCosPA > lCascCosPACut ) lCascCosPACut = lVarCascCosPA;
            }
            //========================================================================
            
            //========================================================================
            //Setting up: Variable V0 CosPA
            Float_t lV0CosPACut = lCascadeResult -> GetCutV0CosPA();
            Float_t lVarV0CosPApar[5];
            lVarV0CosPApar[0] = lCascadeResult->GetCutVarV0CosPAExp0Const();
            lVarV0CosPApar[1] = lCascadeResult->GetCutVarV0CosPAExp0Slope();
            lVarV0CosPApar[2] = lCascadeResult->GetCutVarV0CosPAExp1Const();
            lVarV0CosPApar[3] = lCascadeResult->GetCutVarV0CosPAExp1Slope();
            lVarV0CosPApar[4] = lCascadeResult->GetCutVarV0CosPAConst();
            Float_t lVarV0CosPA = TMath::Cos(
                                             lVarV0CosPApar[0]*TMath::Exp(lVarV0CosPApar[1]*fTreeCascVarPt) +
                                             lVarV0CosPApar[2]*TMath::Exp(lVarV0CosPApar[3]*fTreeCascVarPt) +
                                             lVarV0CosPApar[4]);
            if( lCascadeResult->GetCutUseVarV0CosPA() ){
                //Only use if tighter than the non-variable cut
                if( lVarV0CosPA > lV0CosPACut ) lV0CosPACut = lVarV0CosPA;
            }
            //========================================================================
            
            //========================================================================
            //Setting up: Variable BB CosPA
            Float_t lBBCosPACut = lCascadeResult -> GetCutBachBaryonCosPA();
            Float_t lVarBBCosPApar[5];
            lVarBBCosPApar[0] = lCascadeResult->GetCutVarBBCosPAExp0Const();
            lVarBBCosPApar[1] = lCascadeResult->GetCutVarBBCosPAExp0Slope();
            lVarBBCosPApar[2] = lCascadeResult->GetCutVarBBCosPAExp1Const();
            lVarBBCosPApar[3] = lCascadeResult->GetCutVarBBCosPAExp1Slope();
            lVarBBCosPApar[4] = lCascadeResult->GetCutVarBBCosPAConst();
            Float_t lVarBBCosPA = TMath::Cos(
                                             lVarBBCosPApar[0]*TMath::Exp(lVarBBCosPApar[1]*fTreeCascVarPt) +
                                             lVarBBCosPApar[2]*TMath::Exp(lVarBBCosPApar[3]*fTreeCascVarPt) +
                                             lVarBBCosPApar[4]);
            if( lCascadeResult->GetCutUseVarBBCosPA() ){
                //Only use if looser than the non-variable cut (WARNING: BEWARE INVERSE LOGIC)
                if( lVarBBCosPA > lBBCosPACut ) lBBCosPACut = lVarBBCosPA;
            }
            //========================================================================
            
            //========================================================================
            //Setting up: Variable DCA Casc Dau
            Float_t lDCACascDauCut = lCascadeResult -> GetCutDCACascDaughters();
            Float_t lVarDCACascDaupar[5];
            lVarDCACascDaupar[0] = lCascadeResult->GetCutVarDCACascDauExp0Const();
            lVarDCACascDaupar[1] = lCascadeResult->GetCutVarDCACascDauExp0Slope();
            lVarDCACascDaupar[2] = lCascadeResult->GetCutVarDCACascDauExp1Const();
            lVarDCACascDaupar[3] = lCascadeResult->GetCutVarDCACascDauExp1Slope();
            lVarDCACascDaupar[4] = lCascadeResult->GetCutVarDCACascDauConst();
            Float_t lVarDCACascDau = lVarDCACascDaupar[0]*TMath::Exp(lVarDCACascDaupar[1]*fTreeCascVarPt) +
            lVarDCACascDaupar[2]*TMath::Exp(lVarDCACascDaupar[3]*fTreeCascVarPt) +
            lVarDCACascDaupar[4];
            if( lCascadeResult->GetCutUseVarDCACascDau() ){
                //Loosest: default cut, parametric can go tighter
                if( lVarDCACascDau < lDCACascDauCut ) lDCACascDauCut = lVarDCACascDau;
            }
            //========================================================================
            
            if ( lCascadeResult->GetMassHypothesis() == AliCascadeResult::kXiMinus     ){
                lCharge  = -1;
                lMass    = fTreeCascVarMassAsXi;
                lRap     = fTreeCascVarRapXi;
                lPDGMass = 1.32171;
                lNegdEdx = fTreeCascVarNegNSigmaPion;
                lPosdEdx = fTreeCascVarPosNSigmaProton;
                lBachdEdx= fTreeCascVarBachNSigmaPion;
                lPDGCode = 3312;
                lprpx = fTreeCascVarPosPx;
                lprpy = fTreeCascVarPosPy;
                lprpz = fTreeCascVarPosPz;
                lBaryonTransvMomMCForG3F = fTreeCascVarPosTransvMomentumMC;
            }
            if ( lCascadeResult->GetMassHypothesis() == AliCascadeResult::kXiPlus      ){
                lCharge  = +1;
                lMass    = fTreeCascVarMassAsXi;
                lRap     = fTreeCascVarRapXi;
                lPDGMass = 1.32171;
                lNegdEdx = fTreeCascVarNegNSigmaProton;
                lPosdEdx = fTreeCascVarPosNSigmaPion;
                lBachdEdx= fTreeCascVarBachNSigmaPion;
                lPDGCode = -3312;
                lprpx = fTreeCascVarNegPx;
                lprpy = fTreeCascVarNegPy;
                lprpz = fTreeCascVarNegPz;
                lBaryonTransvMomMCForG3F = fTreeCascVarNegTransvMomentumMC;
            }
            if ( lCascadeResult->GetMassHypothesis() == AliCascadeResult::kOmegaMinus     ){
                lCharge  = -1;
                lMass    = fTreeCascVarMassAsOmega;
                lRap     = fTreeCascVarRapOmega;
                lPDGMass = 1.67245;
                lNegdEdx = fTreeCascVarNegNSigmaPion;
                lPosdEdx = fTreeCascVarPosNSigmaProton;
                lBachdEdx= fTreeCascVarBachNSigmaKaon;
                lPDGCode = 3334;
                lprpx = fTreeCascVarPosPx;
                lprpy = fTreeCascVarPosPy;
                lprpz = fTreeCascVarPosPz;
                lBaryonTransvMomMCForG3F = fTreeCascVarPosTransvMomentumMC;
            }
            if ( lCascadeResult->GetMassHypothesis() == AliCascadeResult::kOmegaPlus      ){
                lCharge  = +1;
                lMass    = fTreeCascVarMassAsOmega;
                lRap     = fTreeCascVarRapOmega;
                lPDGMass = 1.67245;
                lNegdEdx = fTreeCascVarNegNSigmaProton;
                lPosdEdx = fTreeCascVarPosNSigmaPion;
                lBachdEdx= fTreeCascVarBachNSigmaKaon;
                lPDGCode = -3334;
                lprpx = fTreeCascVarNegPx;
                lprpy = fTreeCascVarNegPy;
                lprpz = fTreeCascVarNegPz;
                lBaryonTransvMomMCForG3F = fTreeCascVarNegTransvMomentumMC;
            }
            
            //Override rapidity for true rapidity if requested to do so
            if ( lCascadeResult -> GetCutMCUseMCProperties() ) {
                lRap = fTreeCascVarRapMC;
            }
            if (
                //Check 1: Charge consistent with expectations
                fTreeCascVarCharge == lCharge &&
                
                //Check 2: Basic Acceptance cuts
                lCascadeResult->GetCutMinEtaTracks() < fTreeCascVarPosEta && fTreeCascVarPosEta < lCascadeResult->GetCutMaxEtaTracks() &&
                lCascadeResult->GetCutMinEtaTracks() < fTreeCascVarNegEta && fTreeCascVarNegEta < lCascadeResult->GetCutMaxEtaTracks() &&
                lCascadeResult->GetCutMinEtaTracks() < fTreeCascVarBachEta && fTreeCascVarBachEta < lCascadeResult->GetCutMaxEtaTracks() &&
                lRap > lCascadeResult->GetCutMinRapidity() &&
                lRap < lCascadeResult->GetCutMaxRapidity() &&
                
                //Check 3: Topological Variables
                // - V0 Selections
                fTreeCascVarDCANegToPrimVtx > lCascadeResult->GetCutDCANegToPV() &&
                fTreeCascVarDCAPosToPrimVtx > lCascadeResult->GetCutDCAPosToPV() &&
                fTreeCascVarDCAV0Daughters < lCascadeResult->GetCutDCAV0Daughters() &&
                fTreeCascVarV0CosPointingAngle > lV0CosPACut &&
                fTreeCascVarV0Radius > lCascadeResult->GetCutV0Radius() &&
                // - Cascade Selections
                fTreeCascVarDCAV0ToPrimVtx > lCascadeResult->GetCutDCAV0ToPV() &&
                TMath::Abs(fTreeCascVarV0Mass-1.116) < lCascadeResult->GetCutV0Mass() &&
                fTreeCascVarDCABachToPrimVtx > lCascadeResult->GetCutDCABachToPV() &&
                fTreeCascVarDCACascDaughters < lDCACascDauCut &&
                fTreeCascVarCascCosPointingAngle > lCascCosPACut &&
                fTreeCascVarCascRadius > lCascadeResult->GetCutCascRadius() &&
                
                // - Implementation of a parametric V0 Mass cut if requested
                (
                 ( lCascadeResult->GetCutV0MassSigma() > 50 ) || //anything goes
                 (TMath::Abs( (fTreeCascVarV0Mass-lExpV0Mass) / lExpV0Sigma ) < lCascadeResult->GetCutV0MassSigma() )
                 ) &&
                
                // - Miscellaneous
                fTreeCascVarDistOverTotMom*lPDGMass < lCascadeResult->GetCutProperLifetime() &&
                fTreeCascVarLeastNbrClusters > lCascadeResult->GetCutLeastNumberOfClusters() &&
                
                // - MC specific: either don't associate (if not requested) or associate
                ( ! (lCascadeResult->GetCutMCPhysicalPrimary())    || fTreeCascVarIsPhysicalPrimary == 1     ) &&
                ( ! (lCascadeResult->GetCutMCPDGCodeAssociation()) || fTreeCascVarPID == lPDGCode            ) &&
                
                //Check 4: TPC dEdx selections
                TMath::Abs(lNegdEdx )<lCascadeResult->GetCutTPCdEdx() &&
                TMath::Abs(lPosdEdx )<lCascadeResult->GetCutTPCdEdx() &&
                TMath::Abs(lBachdEdx)<lCascadeResult->GetCutTPCdEdx() &&
                
                //Check 5: Xi rejection for Omega analysis
                ( ( lCascadeResult->GetMassHypothesis() != AliCascadeResult::kOmegaMinus && lCascadeResult->GetMassHypothesis() != AliCascadeResult::kOmegaPlus  ) || ( TMath::Abs( fTreeCascVarMassAsXi - 1.32171 ) > lCascadeResult->GetCutXiRejection() ) )&&
                
                //Check 6: Experimental DCA Bachelor to Baryon cut
                ( fTreeCascVarDCABachToBaryon > lCascadeResult->GetCutDCABachToBaryon() ) &&
                
                //Check 7: Experimental Bach Baryon CosPA
                ( fTreeCascVarWrongCosPA < lBBCosPACut  ) &&
                
                //Check 8: Min/Max V0 Lifetime cut
                ( ( fTreeCascVarV0Lifetime > lCascadeResult->GetCutMinV0Lifetime() ) &&
                 ( fTreeCascVarV0Lifetime < lCascadeResult->GetCutMaxV0Lifetime() ||
                  lCascadeResult->GetCutMaxV0Lifetime() > 1e+3 ) ) &&
                
                //Check 9: kITSrefit track selection if requested
                (
                 ( (fTreeCascVarPosTrackStatus & AliESDtrack::kITSrefit) &&
                  (fTreeCascVarNegTrackStatus & AliESDtrack::kITSrefit) &&
                  (fTreeCascVarBachTrackStatus & AliESDtrack::kITSrefit)
                  )
                 ||
                 !lCascadeResult->GetCutUseITSRefitTracks()
                 )  &&
                
                //Check 10: Max Chi2/Clusters if not absurd
                ( lCascadeResult->GetCutMaxChi2PerCluster()>1e+3 ||
                 fTreeCascVarMaxChi2PerCluster < lCascadeResult->GetCutMaxChi2PerCluster()
                 ) &&
                
                //Check 11: Min Track Length if positive
                ( lCascadeResult->GetCutMinTrackLength()<0 || //this is a bit paranoid...
                 fTreeCascVarMinTrackLength > lCascadeResult->GetCutMinTrackLength()
                 ) &&
                
                //Check 12: Explicit associate-with-bump
                ( ! (lCascadeResult->GetCutMCSelectBump())    || (//Start bump-selection
                                                                  //Case: XiMinus or OmegaMinus
                                                                  (lCharge == -1 &&
                                                                   fTreeCascVarPosLabelMother == fTreeCascVarBachLabelMother &&
                                                                   fTreeCascVarPIDBachelorMother ==  3122)||
                                                                  //Case: XiPlus or OmegaPlus
                                                                  (lCharge == +1 &&
                                                                   fTreeCascVarNegLabelMother == fTreeCascVarBachLabelMother &&
                                                                   fTreeCascVarPIDBachelorMother == -3122)
                                                                  )//End bump-selection
                 )&&
                //Check 12: Check if special V0 CosPA cut used
                //either don't use the cut at all, or make sure it's above threshold
                ( lCascadeResult->GetCutUse276TeVV0CosPA()==kFALSE ||
                 fTreeCascVarV0CosPointingAngle>l276TeVV0CosPA
                 )&&
                
                //Check 13: 3D Cascade DCA to PV
                ( lCascadeResult->GetCutDCACascadeToPV() > 999 ||
                 (TMath::Sqrt(fTreeCascVarCascDCAtoPVz*fTreeCascVarCascDCAtoPVz + fTreeCascVarCascDCAtoPVxy*fTreeCascVarCascDCAtoPVxy)<lCascadeResult->GetCutDCACascadeToPV() )
                 )&&
                
                //Check 14a: Negative track DCA to PV, weighted
                ( lCascadeResult->GetCutDCANegToPVWeighted() < 0 ||
                 (fTreeCascVarDCANegToPrimVtx/TMath::Sqrt(fTreeCascVarNegDCAPVSigmaX2*fTreeCascVarNegDCAPVSigmaX2 + fTreeCascVarNegDCAPVSigmaY2*fTreeCascVarNegDCAPVSigmaY2+1e-6)>lCascadeResult->GetCutDCANegToPVWeighted() )
                 )&&
                //Check 14a: Negative track DCA to PV, weighted
                ( lCascadeResult->GetCutDCAPosToPVWeighted() < 0 ||
                 (fTreeCascVarDCAPosToPrimVtx/TMath::Sqrt(fTreeCascVarPosDCAPVSigmaX2*fTreeCascVarPosDCAPVSigmaX2 + fTreeCascVarPosDCAPVSigmaY2*fTreeCascVarPosDCAPVSigmaY2+1e-6)>lCascadeResult->GetCutDCAPosToPVWeighted() )
                 )&&
                //Check 14a: Negative track DCA to PV, weighted
                ( lCascadeResult->GetCutDCABachToPVWeighted() < 0 ||
                 (fTreeCascVarDCABachToPrimVtx/TMath::Sqrt(fTreeCascVarBachDCAPVSigmaX2*fTreeCascVarBachDCAPVSigmaX2 + fTreeCascVarBachDCAPVSigmaY2*fTreeCascVarBachDCAPVSigmaY2+1e-6)>lCascadeResult->GetCutDCABachToPVWeighted() )
                 )
                )
            {
                
                //This satisfies all my conditionals! Fill histogram
                if( !lCascadeResult -> GetCutMCUseMCProperties() ){
                    histoout -> Fill ( fCentrality, fTreeCascVarPt, lMass );
                    if(histoProtonProfile)
                        histoProtonProfile -> Fill( fTreeCascVarPt, lBaryonTransvMomMCForG3F );
                }else{
                    histoout -> Fill ( fCentrality, fTreeCascVarPtMC, lMass );
                    if(histoProtonProfile)
                        histoProtonProfile -> Fill( fTreeCascVarPtMC, lBaryonTransvMomMCForG3F );
                }
            }
        }
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        // End Superlight adaptive output mode
        //+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        
    }// end of the Cascade loop (ESD or AOD)
    
    // Post output data.
    PostData(1, fListHist    );
    PostData(2, fListV0      );
    PostData(3, fListCascade );
    if( fkSaveEventTree   ) PostData(4, fTreeEvent   );
    if( fkSaveV0Tree      ) PostData(5, fTreeV0      );
    if( fkSaveCascadeTree ) PostData(6, fTreeCascade );
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityMCRun2::Terminate(Option_t *)
{
    // Draw result to the screen
    // Called once at the end of the query
    
    TList *cRetrievedList = 0x0;
    cRetrievedList = (TList*)GetOutputData(1);
    if(!cRetrievedList) {
        Printf("ERROR - AliAnalysisTaskStrangenessVsMultiplicityMCRun2 : ouput data container list not available\n");
        return;
    }
    
    fHistEventCounter = dynamic_cast<TH1D*> (  cRetrievedList->FindObject("fHistEventCounter")  );
    if (!fHistEventCounter) {
        Printf("ERROR - AliAnalysisTaskStrangenessVsMultiplicityMCRun2 : fHistEventCounter not available");
        return;
    }
    
    TCanvas *canCheck = new TCanvas("AliAnalysisTaskStrangenessVsMultiplicityMCRun2","V0 Multiplicity",10,10,510,510);
    canCheck->cd(1)->SetLogy();
    
    fHistEventCounter->SetMarkerStyle(22);
    fHistEventCounter->DrawCopy("E");
}

//________________________________________________________________________
Double_t AliAnalysisTaskStrangenessVsMultiplicityMCRun2::MyRapidity(Double_t rE, Double_t rPz) const
{
    // Local calculation for rapidity
    Double_t ReturnValue = -100;
    if( (rE-rPz+1.e-13) != 0 && (rE+rPz) != 0 ) {
        ReturnValue =  0.5*TMath::Log((rE+rPz)/(rE-rPz+1.e-13));
    }
    return ReturnValue;
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityMCRun2::AddConfiguration( AliV0Result *lV0Result )
{
    if (!fListV0){
        Printf("fListV0 does not exist. Creating...");
        fListV0 = new TList();
        fListV0->SetOwner();
        
    }
    fListV0->Add(lV0Result);
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityMCRun2::AddConfiguration( AliCascadeResult *lCascadeResult )
{
    if (!fListCascade){
        Printf("fListCascade does not exist. Creating...");
        fListCascade = new TList();
        fListCascade->SetOwner();
        
    }
    fListCascade->Add(lCascadeResult);
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityMCRun2::SetupStandardVertexing()
//Meant to store standard re-vertexing configuration
{
    //Tell the task to re-run vertexers
    SetRunVertexers(kTRUE);
    SetDoV0Refit(kTRUE);
    
    //V0-Related topological selections
    SetV0VertexerDCAFirstToPV(0.05);
    SetV0VertexerDCASecondtoPV(0.05);
    SetV0VertexerDCAV0Daughters(1.20);
    SetV0VertexerCosinePA(0.98);
    SetV0VertexerMinRadius(0.9);
    SetV0VertexerMaxRadius(200);
    
    //Cascade-Related topological selections
    SetCascVertexerMinV0ImpactParameter(0.05);
    SetCascVertexerV0MassWindow(0.006);
    SetCascVertexerDCABachToPV(0.02);
    SetCascVertexerDCACascadeDaughters(1.2);
    SetCascVertexerCascadeMinRadius(.8);
    SetCascVertexerCascadeCosinePA(.98);
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityMCRun2::SetupLooseVertexing()
//Meant to store standard re-vertexing configuration
{
    //Tell the task to re-run vertexers
    SetRunVertexers(kTRUE);
    SetDoV0Refit(kTRUE);
    
    //V0-Related topological selections
    SetV0VertexerDCAFirstToPV(0.1);
    SetV0VertexerDCASecondtoPV(0.1);
    SetV0VertexerDCAV0Daughters(1.40);
    SetV0VertexerCosinePA(0.95);
    SetV0VertexerMinRadius(0.9);
    SetV0VertexerMaxRadius(200);
    
    //Cascade-Related topological selections
    SetCascVertexerMinV0ImpactParameter(0.05);
    SetCascVertexerV0MassWindow(0.006);
    SetCascVertexerDCABachToPV(0.02);
    SetCascVertexerDCACascadeDaughters(1.4);
    SetCascVertexerCascadeMinRadius(.5);
    SetCascVertexerCascadeCosinePA(.95);
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityMCRun2::AddTopologicalQAV0(Int_t lRecNumberOfSteps)
//Add all configurations to do QA of topological variables for the V0 analysis
{
    // STEP 1: Decide on binning (needed to improve on memory consumption)
    
    // pT binning
    Double_t lPtbinlimits[] ={0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0,
        2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0, 4.5, 5.0, 5.5, 6.5, 8.0, 10, 12, 15};
    Long_t lPtbinnumb = sizeof(lPtbinlimits)/sizeof(Double_t) - 1;
    
    Double_t lPtbinlimitsCascade[] ={0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0,
        2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0, 4.5, 5.0, 5.5, 6.5, 8.0, 10, 12, 14, 17, 20};
    Long_t lPtbinnumbCascade = sizeof(lPtbinlimitsCascade)/sizeof(Double_t) - 1;
    
    // centrality binning
    Double_t lCentbinlimits[] = {0, 10};
    Long_t lCentbinnumb = sizeof(lCentbinlimits)/sizeof(Double_t) - 1;
    
    // TStrings for output names
    TString lParticleName[] = {"K0Short", "Lambda",  "AntiLambda"};
    
    //STEP 3: Creation of output objects
    
    //Map to mass hypothesis
    AliV0Result::EMassHypo lMassHypoV0[3];
    lMassHypoV0[0] = AliV0Result::kK0Short;
    lMassHypoV0[1] = AliV0Result::kLambda;
    lMassHypoV0[2] = AliV0Result::kAntiLambda;
    
    Float_t lLifetimeCut[3];
    lLifetimeCut[0] = 20.0;
    lLifetimeCut[1] = 30.0;
    lLifetimeCut[2] = 30.0;
    
    Float_t lMass[3];
    lMass[0] = 0.497;
    lMass[1] = 1.116;
    lMass[2] = 1.116;
    
    Float_t lMWindow[3];
    lMWindow[0] = 0.075;
    lMWindow[1] = 0.050;
    lMWindow[2] = 0.050;
    
    //Array of results
    AliV0Result *lV0Result[5000];
    Long_t lNV0 = 0;
    
    //Central results: Stored in indices 0, 1, 2 (careful!)
    for(Int_t i = 0 ; i < 3 ; i ++){
        //Central result, customized binning: the one to use, usually
        lV0Result[lNV0] = new AliV0Result( Form("%s_Central",lParticleName[i].Data() ),lMassHypoV0[i],"",lCentbinnumb,lCentbinlimits, lPtbinnumb,lPtbinlimits, 100,lMass[i]-lMWindow[i],lMass[i]+lMWindow[i]);
        if ( i>0 )
            lV0Result[lNV0]->InitializeFeeddownMatrix( lPtbinnumb, lPtbinlimits, lPtbinnumbCascade, lPtbinlimitsCascade, lCentbinnumb, lCentbinlimits );
        
        //Setters for V0 Cuts
        lV0Result[lNV0]->SetCutDCANegToPV            ( 0.05 ) ;
        lV0Result[lNV0]->SetCutDCAPosToPV            ( 0.05 ) ;
        lV0Result[lNV0]->SetCutDCAV0Daughters        ( 1.2 ) ;
        lV0Result[lNV0]->SetCutV0CosPA               ( 0.98 ) ;
        lV0Result[lNV0]->SetCutV0Radius              ( 0.9 ) ;
        
        //Miscellaneous
        lV0Result[lNV0]->SetCutProperLifetime        ( lLifetimeCut[i] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( 70 ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( 0.8 ) ;
        lV0Result[lNV0]->SetCutTPCdEdx               ( 4 ) ;
        
        //Add result to pool
        lNV0++;
    }
    
    //Will now proceed to sweep individual variables
    //Number of steps used for the variable sweeps
    const Int_t lNumberOfSteps = lRecNumberOfSteps;
    
    //________________________________________________________
    // Variable 1: DCA Neg to PV
    Float_t lMaxDCANegToPV = 20.00;
    
    for(Int_t i = 0 ; i < 3 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCANegToPVSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCANegToPV / ((Float_t) lNumberOfSteps) ;
            lV0Result[lNV0] -> SetCutDCANegToPV ( lThisCut );
            lNV0++;
        }
    }
    //________________________________________________________
    // Variable 2: DCA Pos to PV
    Float_t lMaxDCAPosToPV = 20.00;
    
    for(Int_t i = 0 ; i < 3 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCAPosToPVSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCAPosToPV / ((Float_t) lNumberOfSteps) ;
            lV0Result[lNV0] -> SetCutDCAPosToPV ( lThisCut );
            lNV0++;
        }
    }
    //________________________________________________________
    // Variable 3: DCA V0 Daughters
    Float_t lMaxDCAV0Daughters = 1.20;
    
    for(Int_t i = 0 ; i < 3 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCAV0DaughtersSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCAV0Daughters / ((Float_t) lNumberOfSteps) ;
            lV0Result[lNV0] -> SetCutDCAV0Daughters ( lThisCut );
            lNV0++;
        }
    }
    //________________________________________________________
    // Variable 4: V0 CosPA
    Float_t lMinV0CosPA = 0.98;
    Float_t lMaxV0CosPA = 1.00;
    Double_t lV0CosPAVals[lNumberOfSteps];
    Double_t lMinV0PA = 0.0;
    Double_t lMaxV0PA = TMath::ACos(lMinV0CosPA);
    Double_t lDeltaV0PA = lMaxV0PA / ((Double_t)(lNumberOfSteps));
    for(Int_t iStep = 0; iStep<lNumberOfSteps; iStep++){
        lV0CosPAVals[iStep] = TMath::Cos( ((Float_t)(iStep+1))*lDeltaV0PA );
    }
    for(Int_t i = 0 ; i < 3 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s_%i",lParticleName[i].Data(),"V0CosPASweep",icut) );
            //Add result to pool
            lV0Result[lNV0] -> SetCutV0CosPA ( lV0CosPAVals[icut] );
            lNV0++;
        }
    }
    //________________________________________________________
    // Variable 5: V0 Radius
    Float_t lMinV0Radius = 2.0;
    Float_t lMaxV0Radius = 20.00;
    for(Int_t i = 0 ; i < 3 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s_%i",lParticleName[i].Data(),"V0RadiusSweep",icut) );
            //Add result to pool
            Float_t lThisCut = lMinV0Radius + (lMaxV0Radius-lMinV0Radius)*(((Float_t)icut)+1)/((Float_t)lNumberOfSteps);
            lV0Result[lNV0] -> SetCutV0Radius ( lThisCut );
            lNV0++;
        }
    }
    for (Int_t iconf = 0; iconf<lNV0; iconf++)
        AddConfiguration(lV0Result[iconf]);
    
    cout<<"Added "<<lNV0<<" V0 configurations to output."<<endl;
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityMCRun2::AddTopologicalQACascade(Int_t lRecNumberOfSteps)
//Add all configurations to do QA of topological variables for the V0 analysis
{
    // STEP 1: Decide on binning (needed to improve on memory consumption)
    
    // pT binning
    Double_t lPtbinlimits[] = {0.4, 0.5, 0.6,
        0.7,0.8,.9,1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,2.0,
        2.1,2.2,2.3,2.4,2.5,2.6,2.7,2.8,3.0,3.2,3.4,3.6,3.8,4.0,4.2,
        4.4,4.6,4.8,5.0,5.5,6.0,6.5,7.0,8.0,9.0,10.,11.,12.};
    //Double_t lPtbinlimits[] = {0.2,0.3, 0.4, 0.5, 0.6,
    //    0.7,0.8,.9,1.0,1.2, 1.4, 1.6, 1.8 ,2.0,
    //    2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0,
    //    4.4,4.8,5.0,6.0,7.0,8.0,9.0,10.,11.,12.};
    
    //Double_t lPtbinlimits[] = {0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.4, 2.8, 3.2,
    //3.6, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.5, 8.5, 10, 12};
    
    Long_t lPtbinnumb = sizeof(lPtbinlimits)/sizeof(Double_t) - 1;
    
    // centrality binning
    Double_t lCentbinlimits[] = {0, 10}; //optimize in 0-10%
    Long_t lCentbinnumb = sizeof(lCentbinlimits)/sizeof(Double_t) - 1;
    
    //Just a counter and one array, please. Nothing else needed
    AliCascadeResult *lCascadeResult[5000];
    Long_t lN = 0;
    
    //Map to mass hypothesis
    AliCascadeResult::EMassHypo lMassHypo[4];
    lMassHypo[0] = AliCascadeResult::kXiMinus;
    lMassHypo[1] = AliCascadeResult::kXiPlus;
    lMassHypo[2] = AliCascadeResult::kOmegaMinus;
    lMassHypo[3] = AliCascadeResult::kOmegaPlus;
    
    Float_t lLifetimeCut[4];
    lLifetimeCut[0] = 15.0;
    lLifetimeCut[1] = 15.0;
    lLifetimeCut[2] = 12.0;
    lLifetimeCut[3] = 12.0;
    
    Float_t lMass[4];
    lMass[0] = 1.322;
    lMass[1] = 1.322;
    lMass[2] = 1.672;
    lMass[3] = 1.672;
    
    TString lParticleName[] = {"XiMinus", "XiPlus",  "OmegaMinus", "OmegaPlus"};
    
    //Number of steps used for the variable sweeps
    const Int_t lNumberOfSteps = lRecNumberOfSteps;
    
    //Central results: Stored in indices 0, 1, 2, 3 (careful!)
    for(Int_t i = 0 ; i < 4 ; i ++){
        //Central result, customized binning: the one to use, usually
        lCascadeResult[lN] = new AliCascadeResult( Form("%s_VertexerLevel",lParticleName[i].Data() ),lMassHypo[i],"",lCentbinnumb,lCentbinlimits, lPtbinnumb,lPtbinlimits,100,lMass[i]-0.050,lMass[i]+0.050);
        
        //Default cuts: use vertexer level ones
        //Setters for V0 Cuts
        lCascadeResult[lN]->SetCutDCANegToPV            ( 0.2 ) ;
        lCascadeResult[lN]->SetCutDCAPosToPV            ( 0.2 ) ;
        lCascadeResult[lN]->SetCutDCAV0Daughters        (  1. ) ;
        lCascadeResult[lN]->SetCutV0CosPA               ( 0.95 ) ; //+variable
        lCascadeResult[lN]->SetCutVarV0CosPA            (TMath::Exp(10.853),
                                                         -25.0322,
                                                         TMath::Exp(-0.843948),
                                                         -0.890794,
                                                         0.057553);
        lCascadeResult[lN]->SetCutV0Radius              (  3 ) ;
        //Setters for Cascade Cuts
        lCascadeResult[lN]->SetCutDCAV0ToPV             ( 0.1 ) ;
        lCascadeResult[lN]->SetCutV0Mass                ( 0.006 ) ;
        lCascadeResult[lN]->SetCutDCABachToPV           ( 0.1 ) ;
        lCascadeResult[lN]->SetCutDCACascDaughters      ( 1.0) ;
        lCascadeResult[lN]->SetCutCascRadius            ( 1.2 ) ;
        if(i==2||i==3)
            lCascadeResult[lN]->SetCutCascRadius            ( 1.0 ) ; //omega case
        lCascadeResult[lN]->SetCutCascCosPA             ( 0.95 ) ; //+variable
        lCascadeResult[lN]->SetCutVarCascCosPA          (TMath::Exp(4.86664),
                                                         -10.786,
                                                         TMath::Exp(-1.33411),
                                                         -0.729825,
                                                         0.0695724);
        //Miscellaneous
        lCascadeResult[lN]->SetCutProperLifetime        ( lLifetimeCut[i] ) ;
        lCascadeResult[lN]->SetCutLeastNumberOfClusters ( 70.0 ) ;
        lCascadeResult[lN]->SetCutTPCdEdx               ( 4.0 ) ;
        lCascadeResult[lN]->SetCutXiRejection           ( 0.008 ) ;
        lCascadeResult[lN]->SetCutBachBaryonCosPA       ( TMath::Cos(0.04) ) ; //+variable
        lCascadeResult[lN]->SetCutVarBBCosPA            (TMath::Exp(-2.29048),
                                                         -20.2016,
                                                         TMath::Exp(-2.9581),
                                                         -0.649153,
                                                         0.00526455);
        //Add result to pool
        lN++;
    }
    
    //Will now proceed to sweep individual variables
    
    //________________________________________________________
    // Variable 1: DCA Neg to PV
    Float_t lMaxDCANegToPV = 1.5;
    
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCANegToPVSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCANegToPV / ((Float_t) lNumberOfSteps) ;
            lCascadeResult[lN] -> SetCutDCANegToPV ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 2: DCA Pos to PV
    Float_t lMaxDCAPosToPV = 1.5;
    
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCAPosToPVSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCAPosToPV / ((Float_t) lNumberOfSteps) ;
            lCascadeResult[lN] -> SetCutDCAPosToPV ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 3: DCA V0 Daughters
    Float_t lMaxDCAV0Daughters = 1.40;
    
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCAV0DaughtersSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCAV0Daughters / ((Float_t) lNumberOfSteps) ;
            lCascadeResult[lN] -> SetCutDCAV0Daughters ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 4: V0 CosPA
    Float_t lMinV0CosPA = 0.95;
    Float_t lMaxV0CosPA = 1.00;
    Double_t lV0CosPAVals[lNumberOfSteps];
    Double_t lMinV0PA = 0.0;
    Double_t lMaxV0PA = TMath::ACos(lMinV0CosPA);
    Double_t lDeltaV0PA = lMaxV0PA / ((Double_t)(lNumberOfSteps));
    for(Int_t iStep = 0; iStep<lNumberOfSteps; iStep++){
        lV0CosPAVals[iStep] = TMath::Cos( ((Float_t)(iStep+1))*lDeltaV0PA );
    }
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"V0CosPASweep",icut) );
            //Add result to pool
            lCascadeResult[lN] -> SetCutUseVarV0CosPA( kFALSE );
            lCascadeResult[lN] -> SetCutV0CosPA ( lV0CosPAVals[icut] );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 5: V0 Radius
    Float_t lMinV0Radius = 0.0;
    Float_t lMaxV0Radius = 20.00;
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"V0RadiusSweep",icut) );
            //Add result to pool
            Float_t lThisCut = lMinV0Radius + (lMaxV0Radius-lMinV0Radius)*(((Float_t)icut)+1)/((Float_t)lNumberOfSteps);
            lCascadeResult[lN] -> SetCutV0Radius ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 6:
    Float_t lMaxDCAV0ToPV = 0.5;
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCAV0ToPVSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCAV0ToPV / ((Float_t) lNumberOfSteps) ;
            lCascadeResult[lN] -> SetCutDCAV0ToPV ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 7: DCA Bach To PV
    Float_t lMaxDCABachToPV = 0.5;
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCABachToPVSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCABachToPV / ((Float_t) lNumberOfSteps) ;
            lCascadeResult[lN] -> SetCutDCABachToPV ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 8: DCA Casc Daughters
    Float_t lMaxDCACascDaughters = 1.40;
    
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"DCACascDaughtersSweep",icut) );
            //Add result to pool
            Float_t lThisCut = ((Float_t)icut+1)*lMaxDCACascDaughters / ((Float_t) lNumberOfSteps) ;
            lCascadeResult[lN] -> SetCutDCACascDaughters ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 9: Cascade Radius
    Float_t lMinCascRadius = 0.5;
    Float_t lMaxCascRadius = 7.0;
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"CascRadiusSweep",icut) );
            //Add result to pool
            Float_t lThisCut = lMinCascRadius + (lMaxCascRadius-lMinCascRadius)*(((Float_t)icut)+1)/((Float_t)lNumberOfSteps);
            lCascadeResult[lN] -> SetCutCascRadius ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 10: Cascade CosPA
    Float_t lMinCascCosPA = 0.95;
    Float_t lMaxCascCosPA = 1.00;
    Double_t lCascCosPAVals[lNumberOfSteps];
    Double_t lMinCascPA = 0.0;
    Double_t lMaxCascPA = TMath::ACos(lMinCascCosPA);
    Double_t lDeltaCascPA = lMaxCascPA / ((Double_t)(lNumberOfSteps));
    for(Int_t iStep = 0; iStep<lNumberOfSteps; iStep++){
        lCascCosPAVals[iStep] = TMath::Cos( ((Float_t)(iStep+1))*lDeltaCascPA );
    }
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"CascCosPASweep",icut) );
            //Add result to pool
            lCascadeResult[lN] -> SetCutUseVarCascCosPA( kFALSE );
            lCascadeResult[lN] -> SetCutCascCosPA ( lCascCosPAVals[icut] );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 11: Bach-Baryon CosPA
    Float_t lMinBBCosPA = TMath::Cos(0.1);
    Float_t lMaxBBCosPA = 1.000;
    Double_t lBBCosPAVals[lNumberOfSteps];
    Double_t lMinBBPA = 0.0;
    Double_t lMaxBBPA = TMath::ACos(lMinBBCosPA);
    Double_t lDeltaBBPA = lMaxBBPA / ((Double_t)(lNumberOfSteps));
    for(Int_t iStep = 0; iStep<lNumberOfSteps; iStep++){
        lBBCosPAVals[iStep] = TMath::Cos( ((Float_t)(iStep+1))*lDeltaBBPA );
    }
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lNumberOfSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"BBCosPASweep",icut) );
            //Add result to pool
            lCascadeResult[lN] -> SetCutUseVarBBCosPA( kFALSE );
            lCascadeResult[lN] -> SetCutBachBaryonCosPA ( lBBCosPAVals[icut] );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 12: Cascade Lifetime Sweep
    
    Int_t lLifetimeSteps = 15;
    for(Int_t i = 0 ; i < 4 ; i ++){
        Float_t lMinLifetime = 5.00;
        Float_t lMaxLifetime = 20.00;
        for(Int_t icut = 0; icut<lLifetimeSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"CascLifetimeSweep",icut) );
            Float_t lThisCut = lMinLifetime + (lMaxLifetime-lMinLifetime)*(((Float_t)icut)+1)/((Float_t)lLifetimeSteps);
            //Add result to pool
            lCascadeResult[lN] -> SetCutProperLifetime ( lThisCut );
            lN++;
        }
    }
    //________________________________________________________
    // Variable 13: V0 Lifetime Sweep
    Float_t lMinV0Lifetime = 8.00;
    Float_t lMaxV0Lifetime = 40.00;
    Int_t lV0LifetimeSteps = 32;
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t icut = 0; icut<lV0LifetimeSteps; icut++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%i",lParticleName[i].Data(),"MaxV0LifetimeSweep",icut) );
            Float_t lThisCut = lMinV0Lifetime + (lMaxV0Lifetime-lMinV0Lifetime)*(((Float_t)icut)+1)/((Float_t)lV0LifetimeSteps);
            //Add result to pool
            lCascadeResult[lN] -> SetCutMaxV0Lifetime ( lThisCut );
            lN++;
        }
    }
    
    for (Int_t iconf = 0; iconf<lN; iconf++)
        AddConfiguration(lCascadeResult[iconf]);
    
    cout<<"Added "<<lN<<" Cascade configurations to output."<<endl;
    
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityMCRun2::AddStandardV0Configuration(Bool_t lUseFull)
//Meant to add some standard V0 analysis Configuration + its corresponding systematics
{
    //======================================================
    // V0 Configurations To use
    //======================================================
    
    // STEP 1: Decide on binning (needed to improve on memory consumption)
    
    // pT binning
    Double_t lPtbinlimitsV0[] ={0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0, 4.5, 5.0, 5.5, 6.5, 8.0, 10, 12, 14, 15, 17, 20};
    Long_t lPtbinnumbV0 = sizeof(lPtbinlimitsV0)/sizeof(Double_t) - 1;
    Double_t lPtbinlimitsXi[] ={0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0, 4.5, 5.0, 5.5, 6.5, 8.0, 10, 12, 14, 16, 19, 22, 25};
    Long_t lPtbinnumbXi = sizeof(lPtbinlimitsXi)/sizeof(Double_t) - 1;
    
    // centrality binning
    Double_t lCentbinlimitsV0[] = {0, 1, 5, 10, 20, 30, 40, 50, 60, 70, 80, 85, 90};
    Long_t lCentbinnumbV0 = sizeof(lCentbinlimitsV0)/sizeof(Double_t) - 1;
    
    // TStrings for output names
    TString lParticleNameV0[] =
    {
        "K0Short",
        "Lambda",
        "AntiLambda"
    };
    const Int_t lNPart = 3;
    TString lConfNameV0[] =
    {
        "Loose",
        "Central",
        "Tight"
    };
    const Int_t lNConf = 3;
    TString lCutNameV0[] =
    {
        "DCANegToPV",
        "DCAPosToPV",
        "DCAV0Daughters",
        "V0CosPA",
        "V0Radius",
        "ProperLifetime",
        "TrackLength",
        "LeastNbrCrsOvFind",
        "TPCdEdx",
        "APParameter",
        "V0RadiusMax",
        "LeastNbrCrsRows"
    };
    const Int_t lNCutsForSyst = 10;
    
    // STEP 2: Decide on a set of selections
    
    //1st index: Particle Species
    //2nd index: Loose / Central / Tight
    //3rd index: Number of selection (as ordered above)
    Double_t lcutsV0[lNPart][lNConf][lNCutsForSyst];
    
    //1st index: Particle Species: K0Short, Lambda, AntiLambda
    //2nd index: Loose / Central / Tight: 2%, 5%, 10% signal loss
    Double_t parExp0Const[lNPart][lNConf];
    Double_t parExp0Slope[lNPart][lNConf];
    Double_t parExp1Const[lNPart][lNConf];
    Double_t parExp1Slope[lNPart][lNConf];
    Double_t parConst[lNPart][lNConf];
    
    //=============================================================================================
    // K0SHORT V0 COS PA PARAMETRIZATION
    //---------------------------------------------------------------------------------------------
    //                       LOOSE                         CENTRAL                           TIGHT
    parExp0Const[0][0] =  0.20428;  parExp0Const[0][1] =  0.22692;  parExp0Const[0][2] =  0.28814;
    parExp0Slope[0][0] = -0.73728;  parExp0Slope[0][1] = -1.59317;  parExp0Slope[0][2] = -2.27069;
    parExp1Const[0][0] =  0.09887;  parExp1Const[0][1] =  0.05994;  parExp1Const[0][2] =  0.04320;
    parExp1Slope[0][0] = -0.02822;  parExp1Slope[0][1] = -0.26997;  parExp1Slope[0][2] = -0.29839;
    parConst[0][0] = -0.05302;      parConst[0][1] =  0.00907;      parConst[0][2] =  0.00704;
    //=============================================================================================
    
    //=============================================================================================
    // LAMBDA V0 COS PA PARAMETRIZATION
    //---------------------------------------------------------------------------------------------
    //                       LOOSE                         CENTRAL                           TIGHT
    parExp0Const[1][0] =  0.22775;  parExp0Const[1][1] =  0.36284;  parExp0Const[1][2] =  0.54877;
    parExp0Slope[1][0] = -1.11579;  parExp0Slope[1][1] = -1.87960;  parExp0Slope[1][2] = -2.72912;
    parExp1Const[1][0] =  0.06266;  parExp1Const[1][1] =  0.04543;  parExp1Const[1][2] =  0.03411;
    parExp1Slope[1][0] = -0.17086;  parExp1Slope[1][1] = -0.20447;  parExp1Slope[1][2] = -0.26965;
    parConst[1][0] =  0.01489;      parConst[1][1] =  0.01085;      parConst[1][2] =  0.00889;
    //=============================================================================================
    
    //=============================================================================================
    // ANTILAMBDA V0 COS PA PARAMETRIZATION
    //---------------------------------------------------------------------------------------------
    //                       LOOSE                         CENTRAL                           TIGHT
    parExp0Const[2][0] =  0.22667;  parExp0Const[2][1] =  0.35809;  parExp0Const[2][2] =  0.54114;
    parExp0Slope[2][0] = -0.93618;  parExp0Slope[2][1] = -1.93860;  parExp0Slope[2][2] = -2.71000;
    parExp1Const[2][0] =  0.06857;  parExp1Const[2][1] =  0.05306;  parExp1Const[2][2] =  0.03664;
    parExp1Slope[2][0] = -0.07015;  parExp1Slope[2][1] = -0.24518;  parExp1Slope[2][2] = -0.28124;
    parConst[2][0] = -0.00707;      parConst[2][1] =  0.01213;      parConst[2][2] =  0.00905;
    //=============================================================================================
    
    //================================================================================
    // K0SHORT SELECTIONS
    //--------------------------------------------------------------------------------
    //                  LOOSE                        CENTRAL                   TIGHT
    lcutsV0[0][0][ 0] = 0.05;    lcutsV0[0][1][ 0] =  0.10; lcutsV0[0][2][0] = 0.17; //DCANegToPV
    lcutsV0[0][0][ 1] = 0.05;    lcutsV0[0][1][ 1] =  0.10; lcutsV0[0][2][1] = 0.17; //DCAPosToPV
    lcutsV0[0][0][ 2] = 0.95;    lcutsV0[0][1][ 2] =   0.8; lcutsV0[0][2][2] =  0.7; //DCAV0Daughters
    lcutsV0[0][0][ 3] = 0.95;    lcutsV0[0][1][ 3] =  0.95; lcutsV0[0][2][3] = 0.95; //V0CosPA
    lcutsV0[0][0][ 4] = 4.50;    lcutsV0[0][1][ 4] =  5.00; lcutsV0[0][2][4] = 5.50; //V0Radius
    lcutsV0[0][0][ 5] =   25;    lcutsV0[0][1][ 5] =    20; lcutsV0[0][2][5] =   15; //Proper Lifetime (in cm)
    lcutsV0[0][0][ 6] =   80;    lcutsV0[0][1][ 6] =    90; lcutsV0[0][2][6] =  100; //Track Length
    lcutsV0[0][0][ 7] =  0.7;    lcutsV0[0][1][ 7] =   0.8; lcutsV0[0][2][7] = 0.85; //Least Ratio CrdRows/Findable
    lcutsV0[0][0][ 8] =  4.0;    lcutsV0[0][1][ 8] =   3.0; lcutsV0[0][2][8] =  2.5; //TPC dE/dx
    lcutsV0[0][0][ 9] = 0.18;    lcutsV0[0][1][ 9] =  0.20; lcutsV0[0][2][9] = 0.22; //AP Parameter
    //================================================================================
    
    //================================================================================
    // LAMBDA SELECTIONS
    //--------------------------------------------------------------------------------
    //                  LOOSE                        CENTRAL                   TIGHT
    lcutsV0[1][0][ 0] = 0.10;    lcutsV0[1][1][ 0] =  0.25; lcutsV0[1][2][0] = 0.40; //DCANegToPV
    lcutsV0[1][0][ 1] = 0.08;    lcutsV0[1][1][ 1] =  0.10; lcutsV0[1][2][1] = 0.13; //DCAPosToPV
    lcutsV0[1][0][ 2] =  1.0;    lcutsV0[1][1][ 2] =   0.8; lcutsV0[1][2][2] = 0.65; //DCAV0Daughters
    lcutsV0[1][0][ 3] = 0.97;    lcutsV0[1][1][ 3] =  0.98; lcutsV0[1][2][3] = 0.99; //V0CosPA
    lcutsV0[1][0][ 4] = 4.00;    lcutsV0[1][1][ 4] =  5.00; lcutsV0[1][2][4] = 6.00; //V0Radius
    lcutsV0[1][0][ 5] =   30;    lcutsV0[1][1][ 5] =    25; lcutsV0[1][2][5] =   20; //Proper Lifetime (in cm)
    lcutsV0[1][0][ 6] =   80;    lcutsV0[1][1][ 6] =    90; lcutsV0[1][2][6] =  100; //Track Length
    lcutsV0[1][0][ 7] =  0.7;    lcutsV0[1][1][ 7] =   0.8; lcutsV0[1][2][7] = 0.85; //Least Ratio CrdRows/Findable
    lcutsV0[1][0][ 8] =  4.0;    lcutsV0[1][1][ 8] =   3.0; lcutsV0[1][2][8] =  2.5; //TPC dE/dx
    lcutsV0[1][0][ 9] = 0.18;    lcutsV0[1][1][ 9] =  0.20; lcutsV0[1][2][9] = 0.22; //AP Parameter
    //================================================================================
    
    //================================================================================
    // ANTILAMBDA SELECTIONS
    //--------------------------------------------------------------------------------
    //                  LOOSE                        CENTRAL                   TIGHT
    lcutsV0[2][0][ 0] = 0.08;    lcutsV0[2][1][ 0] =  0.10; lcutsV0[2][2][0] = 0.13; //DCANegToPV
    lcutsV0[2][0][ 1] = 0.10;    lcutsV0[2][1][ 1] =  0.25; lcutsV0[2][2][1] = 0.40; //DCAPosToPV
    lcutsV0[2][0][ 2] =  1.0;    lcutsV0[2][1][ 2] =   0.8; lcutsV0[2][2][2] = 0.65; //DCAV0Daughters
    lcutsV0[2][0][ 3] = 0.97;    lcutsV0[2][1][ 3] =  0.98; lcutsV0[2][2][3] = 0.99; //V0CosPA
    lcutsV0[2][0][ 4] = 4.00;    lcutsV0[2][1][ 4] =  5.00; lcutsV0[2][2][4] = 6.00; //V0Radius
    lcutsV0[2][0][ 5] =   30;    lcutsV0[2][1][ 5] =    25; lcutsV0[2][2][5] =   20; //Proper Lifetime (in cm)
    lcutsV0[2][0][ 6] =   80;    lcutsV0[2][1][ 6] =    90; lcutsV0[2][2][6] =  100; //Track Length
    lcutsV0[2][0][ 7] =  0.7;    lcutsV0[2][1][ 7] =   0.8; lcutsV0[2][2][7] = 0.85; //Least Ratio CrdRows/Findable
    lcutsV0[2][0][ 8] =  4.0;    lcutsV0[2][1][ 8] =   3.0; lcutsV0[2][2][8] =  2.5; //TPC dE/dx
    lcutsV0[2][0][ 9] = 0.18;    lcutsV0[2][1][ 9] =  0.20; lcutsV0[2][2][9] = 0.22; //AP Parameter
    //================================================================================
    
    //STEP 3: Creation of output objects
    
    //Map to mass hypothesis
    AliV0Result::EMassHypo lMassHypoV0[lNPart];
    lMassHypoV0[0] = AliV0Result::kK0Short;
    lMassHypoV0[1] = AliV0Result::kLambda;
    lMassHypoV0[2] = AliV0Result::kAntiLambda;
    
    //Array of results
    AliV0Result *lV0Result[1000];
    Long_t lNV0 = 0;
    
    //Central results: Stored in indices 0, 1, 2 (careful!)
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        //Central result, customized binning: the one to use, usually
        lV0Result[lNV0] = new AliV0Result( Form("%s_Central",lParticleNameV0[i].Data() ),lMassHypoV0[i],"",lCentbinnumbV0,lCentbinlimitsV0, lPtbinnumbV0,lPtbinlimitsV0);
        lV0Result[lNV0] -> InitializeProtonProfile( lPtbinnumbV0,lPtbinlimitsV0 ); //profile
        
        //feeddown matrix
        if ( i!=0 ) lV0Result[lNV0] -> InitializeFeeddownMatrix( lPtbinnumbV0,lPtbinlimitsV0, lPtbinnumbXi,lPtbinlimitsXi, lCentbinnumbV0,lCentbinlimitsV0);
        
        //Setters for V0 Cuts
        lV0Result[lNV0]->SetCutDCANegToPV            ( lcutsV0[i][1][ 0] ) ;
        lV0Result[lNV0]->SetCutDCAPosToPV            ( lcutsV0[i][1][ 1] ) ;
        lV0Result[lNV0]->SetCutDCAV0Daughters        ( lcutsV0[i][1][ 2] ) ;
        lV0Result[lNV0]->SetCutV0CosPA               ( lcutsV0[i][1][ 3] ) ;
        //Set Variable cut
        lV0Result[lNV0]->SetCutVarV0CosPA               ( parExp0Const[i][1], parExp0Slope[i][1], parExp1Const[i][1], parExp1Slope[i][1], parConst[i][1] ) ;
        
        lV0Result[lNV0]->SetCutV0Radius              ( lcutsV0[i][1][ 4] ) ;
        
        //Miscellaneous
        lV0Result[lNV0]->SetCutProperLifetime        ( lcutsV0[i][1][ 5] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( -1 ) ; //no cut here
        lV0Result[lNV0]->SetCutMinTrackLength ( lcutsV0[i][1][ 6] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lcutsV0[i][1][ 7] ) ;
        lV0Result[lNV0]->SetCutTPCdEdx               ( lcutsV0[i][1][ 8] ) ;
        lV0Result[lNV0]->SetCutArmenterosParameter               ( lcutsV0[i][1][ 9] ) ;
        
        //Add result to pool
        lNV0++;
    }
    
    //Central full results
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        //Central Result, Full: No rebinning. Will use a significant amount of memory,
        //not to be replicated several times for systematics!
        lV0Result[lNV0] = new AliV0Result( Form("%s_Central_Full",lParticleNameV0[i].Data() ),lMassHypoV0[i]);
        lV0Result[lNV0] -> InitializeProtonProfile( lPtbinnumbV0,lPtbinlimitsV0 ); //profile
        
        //feeddown matrix
        if ( i!=0 ) lV0Result[lNV0] -> InitializeFeeddownMatrix( lPtbinnumbV0,lPtbinlimitsV0, lPtbinnumbXi,lPtbinlimitsXi, lCentbinnumbV0,lCentbinlimitsV0);
        
        //Setters for V0 Cuts
        lV0Result[lNV0]->SetCutDCANegToPV            ( lcutsV0[i][1][ 0] ) ;
        lV0Result[lNV0]->SetCutDCAPosToPV            ( lcutsV0[i][1][ 1] ) ;
        lV0Result[lNV0]->SetCutDCAV0Daughters        ( lcutsV0[i][1][ 2] ) ;
        lV0Result[lNV0]->SetCutV0CosPA               ( lcutsV0[i][1][ 3] ) ;
        //Set Variable cut
        lV0Result[lNV0]->SetCutVarV0CosPA               ( parExp0Const[i][1], parExp0Slope[i][1], parExp1Const[i][1], parExp1Slope[i][1], parConst[i][1] ) ;
        
        lV0Result[lNV0]->SetCutV0Radius              ( lcutsV0[i][1][ 4] ) ;
        
        //Miscellaneous
        lV0Result[lNV0]->SetCutProperLifetime        ( lcutsV0[i][1][ 5] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( -1 ) ; //no cut here
        lV0Result[lNV0]->SetCutMinTrackLength ( lcutsV0[i][1][ 6] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lcutsV0[i][1][ 7] ) ;
        lV0Result[lNV0]->SetCutTPCdEdx               ( lcutsV0[i][1][ 8] ) ;
        
        //Add result to pool
        lNV0++;
    }
    
    //Rapidity sweep
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        for(Int_t ir=0; ir<12; ir++)
        {
            Float_t lLoRap = -0.6 +   (ir)*0.1;
            Float_t lHiRap = -0.6 + (ir+1)*0.1;
            
            //Create a new object from default
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_RapiditySweep_%.1f_%.1f",lParticleNameV0[i].Data(),lLoRap, lHiRap) );
            lV0Result[lNV0] -> SetCutMinRapidity( lLoRap );
            lV0Result[lNV0] -> SetCutMaxRapidity( lHiRap );
            
            //Add result to pool
            lNV0++;
        }
    }
    
    //Number of crossed rows cut
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        //Create a new object from default
        lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s",lParticleNameV0[i].Data(),"NCrossedRowsCut") );
        
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( 70 ) ;
        lV0Result[lNV0]->SetCutMinTrackLength ( -1 ) ;
        
        //Add result to pool
        lNV0++;
    }
    
    //No Armenteros-Podolanski cut
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        //Create a new object from default
        lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s",lParticleNameV0[i].Data(),"NoAP") );
        lV0Result[lNV0]->SetCutArmenterosParameter(0.0);
        
        //Add result to pool
        lNV0++;
    }
    
    //Explore Use MC Properties vs Use Reconstructed Properties
    for(Int_t i = 0 ; i < lNPart ; i ++){
        //Create a new object from default
        lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_Central_MCUseRecoProp",lParticleNameV0[i].Data() ) );
        
        lV0Result[lNV0]->SetCutMCUseMCProperties(kFALSE);
        
        //Add result to pool
        lNV0++;
    }
    
    //================================================================================
    //Set up cut values, tight and loose versions
    //--------------------------------------------------------------------------------
    const Int_t lNCutsForSweep = 12;
    //1st index: Particle Species
    //2nd index: Number of selection
    Double_t lCutsTight[lNPart][lNCutsForSweep];
    Double_t lCutsLoose[lNPart][lNCutsForSweep];
    
    Double_t lMeanLifetime[] = {2.6844, 7.89, 7.89};
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        lCutsTight[i][ 0] =   0.1; //DCANegToPV
        lCutsTight[i][ 1] =   0.1; //DCAPosToPV
        lCutsTight[i][ 2] =     1; //DCAV0Daughters
        lCutsTight[i][ 3] = 0.998; //V0CosPA
        lCutsTight[i][ 4] =     5; //V0Radius
        lCutsTight[i][ 5] = 3 * lMeanLifetime[i]; //Proper Lifetime (in cm)
        lCutsTight[i][ 6] =    -1; //Track Length
        lCutsTight[i][ 7] = -0.01; //Least Ratio CrdRows/Findable
        lCutsTight[i][ 8] =     8; //TPC dE/dx
        lCutsTight[i][ 9] =   0.2; //AP Parameter
        lCutsTight[i][10] =   100; //V0Radius max
        lCutsTight[i][11] =    70; //Least number of CrdRows
        
        for(Int_t j = 0; j < lNCutsForSyst; j++)
        {
            lCutsLoose[i][j] = lcutsV0[i][1][j];
        }
        lCutsLoose[i][10] =   200; //V0Radius max
        lCutsLoose[i][11] =    -1; //Least number of CrdRows
    }
    //================================================================================
    
    //2.76 TeV analysis cuts
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_276Cuts",lParticleNameV0[i].Data() ) );
        
        //Setters for V0 Cuts
        lV0Result[lNV0]->SetCutDCANegToPV            ( lCutsTight[i][0] ) ;
        lV0Result[lNV0]->SetCutDCAPosToPV            ( lCutsTight[i][1] ) ;
        lV0Result[lNV0]->SetCutDCAV0Daughters        ( lCutsTight[i][2] ) ;
        lV0Result[lNV0]->SetCutV0CosPA               ( lCutsTight[i][3] ) ;
        //Use constant cos(PA) cut
        lV0Result[lNV0]->SetCutUseVarV0CosPA         ( kFALSE ) ;
        
        lV0Result[lNV0]->SetCutV0Radius              ( lCutsTight[i][4] ) ;
        lV0Result[lNV0]->SetCutMaxV0Radius           ( lCutsTight[i][10] ) ;
        
        //Miscellaneous
        lV0Result[lNV0]->SetCutProperLifetime        ( lCutsTight[i][5] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( lCutsTight[i][11] ) ;
        lV0Result[lNV0]->SetCutMinTrackLength ( lCutsTight[i][6] ) ; //no cut here
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lCutsTight[i][7] ) ;
        lV0Result[lNV0]->SetCutTPCdEdx               ( 1e6 ) ; //no cut here
        lV0Result[lNV0]->SetCut276TeVLikedEdx        ( kTRUE ) ;
        lV0Result[lNV0]->SetCutArmenterosParameter               ( lCutsTight[i][9] ) ;
        
        //Add result to pool
        lNV0++;
    }
    
    // centrality binning for sweeps
    Double_t lSweepCentBinLimits[] = {0, 90};
    Long_t lSweepCentBinNumb = sizeof(lSweepCentBinLimits)/sizeof(Double_t) - 1;
    
    //Mass binning for sweeps
    Double_t lNMassBins[] = {400, 400, 400};
    Double_t lMass[] = {0.498, 1.116, 1.116};
    Double_t lMassWindow[] = {0.15,0.1,0.1};
    
    //Loose cuts for sweeps
    Int_t lLooseForSweepIndex = lNV0;
    for(Int_t i = 0 ; i < lNPart ; i++)
    {
        //Central result for sweeps
        lV0Result[lNV0] = new AliV0Result( Form("%s_Central_ForSweep",lParticleNameV0[i].Data() ),lMassHypoV0[i],"",lSweepCentBinNumb,lSweepCentBinLimits, lPtbinnumbV0,lPtbinlimitsV0,lNMassBins[i],lMass[i]-lMassWindow[i],lMass[i]+lMassWindow[i]);
        lV0Result[lNV0] -> InitializeProtonProfile( lPtbinnumbV0,lPtbinlimitsV0 ); //profile
        
        //feeddown matrix
        if ( i!=0 ) lV0Result[lNV0] -> InitializeFeeddownMatrix( lPtbinnumbV0,lPtbinlimitsV0, lPtbinnumbXi,lPtbinlimitsXi, lSweepCentBinNumb,lSweepCentBinLimits);
        
        //Setters for V0 Cuts
        lV0Result[lNV0]->SetCutDCANegToPV            ( lcutsV0[i][1][ 0] ) ;
        lV0Result[lNV0]->SetCutDCAPosToPV            ( lcutsV0[i][1][ 1] ) ;
        lV0Result[lNV0]->SetCutDCAV0Daughters        ( lcutsV0[i][1][ 2] ) ;
        lV0Result[lNV0]->SetCutV0CosPA               ( lcutsV0[i][1][ 3] ) ;
        //Set Variable cut
        lV0Result[lNV0]->SetCutVarV0CosPA               ( parExp0Const[i][1], parExp0Slope[i][1], parExp1Const[i][1], parExp1Slope[i][1], parConst[i][1] ) ;
        
        lV0Result[lNV0]->SetCutV0Radius              ( lcutsV0[i][1][ 4] ) ;
        
        //Miscellaneous
        lV0Result[lNV0]->SetCutProperLifetime        ( lcutsV0[i][1][ 5] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( -1 ) ; //no cut here
        lV0Result[lNV0]->SetCutMinTrackLength ( lcutsV0[i][1][ 6] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lcutsV0[i][1][ 7] ) ;
        lV0Result[lNV0]->SetCutTPCdEdx               ( lcutsV0[i][1][ 8] ) ;
        lV0Result[lNV0]->SetCutArmenterosParameter               ( lcutsV0[i][1][ 9] ) ;
        
        //Add result to pool
        lNV0++;
    }
    
    //Tightening cuts one by one
    for(Int_t i = 0 ; i < lNPart ; i++)
    {
        for(Int_t iCut = 0; iCut < lNCutsForSweep; iCut++)
        {
            //only proceed if cuts are actually different
            if( ( TMath::Abs( lCutsTight[i][iCut]-lCutsLoose[i][iCut] ) / lCutsLoose[i][iCut] < 0.01 ) && ( iCut != 3 ) ) continue;
            
            Int_t lNSweep = 12;
            for(Int_t iSweep = 1; iSweep <= lNSweep; iSweep++)
            {
                Double_t lCutValue = lCutsLoose[i][iCut] + (iSweep/(Double_t)lNSweep)*(lCutsTight[i][iCut]-lCutsLoose[i][iCut]);
                
                lV0Result[lNV0] = new AliV0Result( lV0Result[lLooseForSweepIndex+i], Form( "%s_Central_%s_%d", lParticleNameV0[i].Data(), lCutNameV0[iCut].Data(), iSweep ) );
                
                if(iCut ==  0 ) lV0Result[lNV0]->SetCutDCANegToPV            ( lCutValue ) ;
                if(iCut ==  1 ) lV0Result[lNV0]->SetCutDCAPosToPV            ( lCutValue ) ;
                if(iCut ==  2 ) lV0Result[lNV0]->SetCutDCAV0Daughters        ( lCutValue ) ;
                if(iCut ==  3 )
                {
                    lV0Result[lNV0]->SetCutV0CosPA               ( lCutValue ) ;
                    lV0Result[lNV0]->SetCutVarV0CosPA            ( parExp0Const[i][1]*(1-iSweep/(Double_t)lNSweep), parExp0Slope[i][1], parExp1Const[i][1]*(1-iSweep/(Double_t)lNSweep), parExp1Slope[i][1], parConst[i][1] + (iSweep/(Double_t)lNSweep)*(TMath::ACos(lCutsTight[i][iCut])-parConst[i][1]) ) ;
                }
                if(iCut ==  4 ) lV0Result[lNV0]->SetCutV0Radius              ( lCutValue ) ;
                
                //Miscellaneous
                if(iCut ==  5 ) lV0Result[lNV0]->SetCutProperLifetime        ( lCutValue ) ;
                if(iCut ==  6 ) lV0Result[lNV0]->SetCutMinTrackLength ( lCutValue ) ;
                if(iCut ==  7 ) lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lCutValue ) ;
                if(iCut ==  8 ) lV0Result[lNV0]->SetCutTPCdEdx               ( lCutValue ) ;
                if(iCut ==  9 ) lV0Result[lNV0]->SetCutArmenterosParameter   ( lCutValue ) ;
                if(iCut == 10 ) lV0Result[lNV0]->SetCutMaxV0Radius           ( lCutValue ) ;
                if(iCut == 11 ) lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( lCutValue ) ;
                
                //Print this variation, add to pool
                lV0Result[lNV0]->Print();
                lNV0++;
            }
        }
    }
    
    //Tight cuts for sweeps
    Int_t lTightForSweepIndex = lNV0;
    for(Int_t i = 0 ; i < lNPart ; i++)
    {
        lV0Result[lNV0] = new AliV0Result( lV0Result[lLooseForSweepIndex+i], Form("%s_276Cuts_ForSweep",lParticleNameV0[i].Data() ) );
        
        //Setters for V0 Cuts
        lV0Result[lNV0]->SetCutDCANegToPV            ( lCutsTight[i][0] ) ;
        lV0Result[lNV0]->SetCutDCAPosToPV            ( lCutsTight[i][1] ) ;
        lV0Result[lNV0]->SetCutDCAV0Daughters        ( lCutsTight[i][2] ) ;
        lV0Result[lNV0]->SetCutV0CosPA               ( lCutsTight[i][3] ) ;
        //Use constant cos(PA) cut
        lV0Result[lNV0]->SetCutUseVarV0CosPA         ( kFALSE ) ;
        
        lV0Result[lNV0]->SetCutV0Radius              ( lCutsTight[i][4] ) ;
        lV0Result[lNV0]->SetCutMaxV0Radius           ( lCutsTight[i][10] ) ;
        
        //Miscellaneous
        lV0Result[lNV0]->SetCutProperLifetime        ( lCutsTight[i][5] ) ;
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( lCutsTight[i][11] ) ;
        lV0Result[lNV0]->SetCutMinTrackLength ( lCutsTight[i][6] ) ; //no cut here
        lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lCutsTight[i][7] ) ;
        lV0Result[lNV0]->SetCutTPCdEdx               ( 1e6 ) ; //no cut here
        lV0Result[lNV0]->SetCut276TeVLikedEdx        ( kTRUE ) ;
        lV0Result[lNV0]->SetCutArmenterosParameter               ( lCutsTight[i][9] ) ;
        
        //Add result to pool
        lNV0++;
    }
    
    //Loosening cuts one by one
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        for(Int_t iCut = 0; iCut < lNCutsForSweep; iCut++)
        {
            //only proceed if cuts are actually different
            if( ( TMath::Abs( lCutsTight[i][iCut]-lCutsLoose[i][iCut] ) / lCutsLoose[i][iCut] < 0.01 ) && ( iCut != 3 ) ) continue;
            
            Int_t lNSweep = 12;
            for(Int_t iSweep = 1; iSweep <= lNSweep; iSweep++)
            {
                Double_t lCutValue = lCutsTight[i][iCut] + (iSweep/(Double_t)lNSweep)*(lCutsLoose[i][iCut]-lCutsTight[i][iCut]);
                
                lV0Result[lNV0] = new AliV0Result( lV0Result[lTightForSweepIndex+i], Form( "%s_276Cuts_%s_%d", lParticleNameV0[i].Data(), lCutNameV0[iCut].Data(), iSweep ) );
                
                if(iCut ==  0 ) lV0Result[lNV0]->SetCutDCANegToPV            ( lCutValue ) ;
                if(iCut ==  1 ) lV0Result[lNV0]->SetCutDCAPosToPV            ( lCutValue ) ;
                if(iCut ==  2 ) lV0Result[lNV0]->SetCutDCAV0Daughters        ( lCutValue ) ;
                if(iCut ==  3 )
                {
                    lV0Result[lNV0]->SetCutV0CosPA               ( lCutValue ) ;
                    lV0Result[lNV0]->SetCutVarV0CosPA            ( parExp0Const[i][1]*(iSweep/(Double_t)lNSweep), parExp0Slope[i][1], parExp1Const[i][1]*(iSweep/(Double_t)lNSweep), parExp1Slope[i][1], TMath::ACos(lCutsTight[i][iCut]) + (iSweep/(Double_t)lNSweep)*(parConst[i][1]-TMath::ACos(lCutsTight[i][iCut])) ) ;
                }
                if(iCut ==  4 ) lV0Result[lNV0]->SetCutV0Radius              ( lCutValue ) ;
                
                //Miscellaneous
                if(iCut ==  5 ) lV0Result[lNV0]->SetCutProperLifetime        ( lCutValue ) ;
                if(iCut ==  6 ) lV0Result[lNV0]->SetCutMinTrackLength ( lCutValue ) ;
                if(iCut ==  7 ) lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lCutValue ) ;
                if(iCut ==  8 )
                {
                    lV0Result[lNV0]->SetCut276TeVLikedEdx        ( kTRUE ) ;
                    lV0Result[lNV0]->SetCutTPCdEdx               ( lCutValue ) ;
                }
                if(iCut ==  9 ) lV0Result[lNV0]->SetCutArmenterosParameter   ( lCutValue ) ;
                if(iCut == 10 ) lV0Result[lNV0]->SetCutMaxV0Radius           ( lCutValue ) ;
                if(iCut == 11 ) lV0Result[lNV0]->SetCutLeastNumberOfCrossedRows ( lCutValue ) ;
                
                //Print this variation, add to pool
                lV0Result[lNV0]->Print();
                lNV0++;
            }
        }
    }
    
    // STEP 4: Creation of objects to be used in systematics
    // Optimized via use of copy constructors
    for(Int_t i = 0 ; i < lNPart ; i ++)
    {
        for(Int_t iCut = 0 ; iCut < lNCutsForSyst ; iCut ++)
        {
            //LOOSE VARIATIONS
            //Create a new object from default
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s_%s",lParticleNameV0[i].Data(),lCutNameV0[iCut].Data(),lConfNameV0[0].Data()) );
            
            if(iCut ==  0 ) lV0Result[lNV0]->SetCutDCANegToPV            ( lcutsV0[i][0][iCut] ) ;
            if(iCut ==  1 ) lV0Result[lNV0]->SetCutDCAPosToPV            ( lcutsV0[i][0][iCut] ) ;
            if(iCut ==  2 ) lV0Result[lNV0]->SetCutDCAV0Daughters        ( lcutsV0[i][0][iCut] ) ;
            if(iCut ==  3 )
            {
                lV0Result[lNV0]->SetCutV0CosPA               ( lcutsV0[i][0][iCut] ) ;
                lV0Result[lNV0]->SetCutVarV0CosPA               ( parExp0Const[i][0], parExp0Slope[i][0], parExp1Const[i][0], parExp1Slope[i][0], parConst[i][0] ) ;
            }
            if(iCut ==  4 ) lV0Result[lNV0]->SetCutV0Radius              ( lcutsV0[i][0][iCut] ) ;
            
            //Miscellaneous
            if(iCut ==  5 ) lV0Result[lNV0]->SetCutProperLifetime        ( lcutsV0[i][0][iCut] ) ;
            if(iCut ==  6 ) lV0Result[lNV0]->SetCutMinTrackLength ( lcutsV0[i][0][ iCut] ) ;
            if(iCut ==  7 ) lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lcutsV0[i][0][iCut] ) ;
            if(iCut ==  8 ) lV0Result[lNV0]->SetCutTPCdEdx               ( lcutsV0[i][0][iCut] ) ;
            if(iCut ==  9 ) lV0Result[lNV0]->SetCutArmenterosParameter               ( lcutsV0[i][0][iCut] ) ;
            
            //Print this variation, add to pool
            lV0Result[lNV0]->Print();
            lNV0++;
            
            //TIGHT VARIATIONS
            //Create a new object from default
            lV0Result[lNV0] = new AliV0Result( lV0Result[i], Form("%s_%s_%s",lParticleNameV0[i].Data(),lCutNameV0[iCut].Data(),lConfNameV0[2].Data()) );
            
            if(iCut ==  0 ) lV0Result[lNV0]->SetCutDCANegToPV            ( lcutsV0[i][2][iCut] ) ;
            if(iCut ==  1 ) lV0Result[lNV0]->SetCutDCAPosToPV            ( lcutsV0[i][2][iCut] ) ;
            if(iCut ==  2 ) lV0Result[lNV0]->SetCutDCAV0Daughters        ( lcutsV0[i][2][iCut] ) ;
            if(iCut ==  3 )
            {
                lV0Result[lNV0]->SetCutV0CosPA               ( lcutsV0[i][2][iCut] ) ;
                lV0Result[lNV0]->SetCutVarV0CosPA               ( parExp0Const[i][2], parExp0Slope[i][2], parExp1Const[i][2], parExp1Slope[i][2], parConst[i][2] ) ;
            }
            if(iCut ==  4 ) lV0Result[lNV0]->SetCutV0Radius              ( lcutsV0[i][2][iCut] ) ;
            
            //Miscellaneous
            if(iCut ==  5 ) lV0Result[lNV0]->SetCutProperLifetime        ( lcutsV0[i][2][iCut] ) ;
            if(iCut ==  6 ) lV0Result[lNV0]->SetCutMinTrackLength ( lcutsV0[i][2][ iCut] ) ;
            if(iCut ==  7 ) lV0Result[lNV0]->SetCutLeastNumberOfCrossedRowsOverFindable               ( lcutsV0[i][2][iCut] ) ;
            if(iCut ==  8 ) lV0Result[lNV0]->SetCutTPCdEdx               ( lcutsV0[i][2][iCut] ) ;
            if(iCut ==  9 ) lV0Result[lNV0]->SetCutArmenterosParameter               ( lcutsV0[i][2][iCut] ) ;
            
            //Print this variation, add to pool
            lV0Result[lNV0]->Print();
            lNV0++;
        }
    }
    
    for (Int_t iconf = 0; iconf<lNV0; iconf++){
        cout<<"Adding config named"<<lV0Result[iconf]->GetName()<<endl;
        AddConfiguration(lV0Result[iconf]);
    }
    
    cout<<"Added "<<lNV0<<" V0 configurations to output."<<endl;
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityMCRun2::AddStandardCascadeConfiguration(Bool_t lUseFull)
//Meant to add some standard cascade analysis Configuration + its corresponding systematics
{
    // STEP 1: Decide on binning (needed to improve on memory consumption)
    
    // pT binning
    Double_t lPtbinlimits[] = {0.4, 0.5, 0.6,
        0.7,0.8,.9,1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,2.0,
        2.1,2.2,2.3,2.4,2.5,2.6,2.7,2.8,3.0,3.2,3.4,3.6,3.8,4.0,4.2,
        4.4,4.5,4.6,4.8,5.0,5.5,6.0,6.5,7.0,8.0,9.0,10.,11.,12.};
    Long_t lPtbinnumb = sizeof(lPtbinlimits)/sizeof(Double_t) - 1;
    
    // centrality binning
    Double_t lCentbinlimits[] = {0, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90};
    Long_t lCentbinnumb = sizeof(lCentbinlimits)/sizeof(Double_t) - 1;
    
    // TStrings for output names
    TString lParticleName[] = {"XiMinus", "XiPlus",  "OmegaMinus", "OmegaPlus"};
    TString lConfName[]     = {"Loose",   "Central", "Tight"     };
    TString lCutName[]      = {
        "DCANegToPV", //1
        "DCAPosToPV", //2
        "DCAV0Daughters", //3
        "V0Radius", //4
        "DCAV0ToPV", //5
        "V0Mass", //6
        "DCABachToPV", //7
        "DCACascDaughters", //8
        "CascRadius", //9
        "ProperLifetime", //10
        "ProperLifetimeV0", //11
        "MinLength", //12
        "TPCdEdx", //13
        "Competing", //14
        "DCA3DCascToPV" //15
    };
    
    // STEP 2: Decide on a set of selections
    
    //1st index: Particle Species
    //2nd index: Loose / Central / Tight
    //3rd index: Number of selection (as ordered above)
    Double_t lcuts[4][3][15];
    
    //N.B.: These are mostly symmetric, except for the proper lifetime, which is different
    //      for the two particle species. Another potential improvement could be asymmetric
    //      DCA selections for the Neg / Pos tracks for the (anti)Lambda decay, as decay
    //      kinematics would prefer having the pion daughter with a larger DCA.
    
    Int_t lIdx = 0;
    
    //================================================================================
    // XIMINUS SELECTIONS
    lIdx = 0; //Master XiMinus Index
    //--------------------------------------------------------------------------------
    //                  LOOSE                        CENTRAL                   TIGHT
    lcuts[lIdx][0][ 0] = 0.10;    lcuts[lIdx][1][ 0] =  0.20; lcuts[lIdx][2][ 0] =  0.30; //DCANegToPV 1
    lcuts[lIdx][0][ 1] = 0.10;    lcuts[lIdx][1][ 1] =  0.20; lcuts[lIdx][2][ 1] =  0.30; //DCAPostToPV 2
    lcuts[lIdx][0][ 2] =  1.2;    lcuts[lIdx][1][ 2] =   1.0; lcuts[lIdx][2][ 2] =   0.8; //DCAV0Daughters 3
    lcuts[lIdx][0][ 3] = 2.00;    lcuts[lIdx][1][ 3] =  3.00; lcuts[lIdx][2][ 3] =   4.0; //V0Radius 4
    lcuts[lIdx][0][ 4] = 0.05;    lcuts[lIdx][1][ 4] =   0.1; lcuts[lIdx][2][ 4] =  0.15; //DCAV0ToPV 5
    lcuts[lIdx][0][ 5] =0.006;    lcuts[lIdx][1][ 5] = 0.005; lcuts[lIdx][2][ 5] = 0.004; //V0Mass 6
    lcuts[lIdx][0][ 6] = 0.05;    lcuts[lIdx][1][ 6] =  0.10; lcuts[lIdx][2][ 6] =  0.15; //DCABachToPV 7
    lcuts[lIdx][0][ 7] = 1.20;    lcuts[lIdx][1][ 7] =   1.0; lcuts[lIdx][2][ 7] =   0.8; //DCACascDaughters 8
    lcuts[lIdx][0][ 8] =  0.8;    lcuts[lIdx][1][ 8] =   1.2; lcuts[lIdx][2][ 8] =  3.00; //CascRadius 9
    lcuts[lIdx][0][ 9] = 17.5;    lcuts[lIdx][1][ 9] =  15.0; lcuts[lIdx][2][ 9] =  12.5; //ProperLifetime 10
    lcuts[lIdx][0][10] = 40.0;    lcuts[lIdx][1][10] =  30.0; lcuts[lIdx][2][10] =  20.0; //ProperLifetimeV0 11
    lcuts[lIdx][0][11] =   80;    lcuts[lIdx][1][11] =    90; lcuts[lIdx][2][11] =   100; //MinimumTrackLength 12
    lcuts[lIdx][0][12] =    5;    lcuts[lIdx][1][12] =     4; lcuts[lIdx][2][12] =     3; //TPCdEdx 13
    lcuts[lIdx][0][13] =  0.0;    lcuts[lIdx][1][13] = 0.008; lcuts[lIdx][2][13] = 0.010; //Competing 14
    lcuts[lIdx][0][14] =  1.2;    lcuts[lIdx][1][14] =   0.8; lcuts[lIdx][2][14] =   0.6; //3D DCA Cascade To PV
    //================================================================================
    
    //================================================================================
    // XIPLUS SELECTIONS
    lIdx = 1; //Master XiPlus Index
    //--------------------------------------------------------------------------------
    //                  LOOSE                        CENTRAL                   TIGHT
    lcuts[lIdx][0][ 0] = 0.10;    lcuts[lIdx][1][ 0] =  0.20; lcuts[lIdx][2][ 0] =  0.30; //DCANegToPV 1
    lcuts[lIdx][0][ 1] = 0.10;    lcuts[lIdx][1][ 1] =  0.20; lcuts[lIdx][2][ 1] =  0.30; //DCAPostToPV 2
    lcuts[lIdx][0][ 2] =  1.2;    lcuts[lIdx][1][ 2] =   1.0; lcuts[lIdx][2][ 2] =   0.8; //DCAV0Daughters 3
    lcuts[lIdx][0][ 3] = 2.00;    lcuts[lIdx][1][ 3] =  3.00; lcuts[lIdx][2][ 3] =   4.0; //V0Radius 4
    lcuts[lIdx][0][ 4] = 0.05;    lcuts[lIdx][1][ 4] =   0.1; lcuts[lIdx][2][ 4] =  0.15; //DCAV0ToPV 5
    lcuts[lIdx][0][ 5] =0.006;    lcuts[lIdx][1][ 5] = 0.005; lcuts[lIdx][2][ 5] = 0.004; //V0Mass 6
    lcuts[lIdx][0][ 6] = 0.05;    lcuts[lIdx][1][ 6] =  0.10; lcuts[lIdx][2][ 6] =  0.15; //DCABachToPV 7
    lcuts[lIdx][0][ 7] = 1.20;    lcuts[lIdx][1][ 7] =   1.0; lcuts[lIdx][2][ 7] =   0.8; //DCACascDaughters 8
    lcuts[lIdx][0][ 8] =  0.8;    lcuts[lIdx][1][ 8] =   1.2; lcuts[lIdx][2][ 8] =  3.00; //CascRadius 9
    lcuts[lIdx][0][ 9] = 17.5;    lcuts[lIdx][1][ 9] =  15.0; lcuts[lIdx][2][ 9] =  12.5; //ProperLifetime 10
    lcuts[lIdx][0][10] = 40.0;    lcuts[lIdx][1][10] =  30.0; lcuts[lIdx][2][10] =  20.0; //ProperLifetimeV0 11
    lcuts[lIdx][0][11] =   80;    lcuts[lIdx][1][11] =    90; lcuts[lIdx][2][11] =   100; //MinimumTrackLength 12
    lcuts[lIdx][0][12] =    5;    lcuts[lIdx][1][12] =     4; lcuts[lIdx][2][12] =     3; //TPCdEdx 13
    lcuts[lIdx][0][13] =  0.0;    lcuts[lIdx][1][13] = 0.008; lcuts[lIdx][2][13] = 0.010; //Competing 14
    lcuts[lIdx][0][14] =  1.2;    lcuts[lIdx][1][14] =   0.8; lcuts[lIdx][2][14] =   0.6; //3D DCA Cascade To PV
    //================================================================================
    
    //================================================================================
    // OMEGAMINUS SELECTIONS
    lIdx = 2; //Master OmegaMinus Index
    //--------------------------------------------------------------------------------
    //                  LOOSE                        CENTRAL                   TIGHT
    lcuts[lIdx][0][ 0] = 0.10;    lcuts[lIdx][1][ 0] =  0.20; lcuts[lIdx][2][ 0] =  0.30; //DCANegToPV 1
    lcuts[lIdx][0][ 1] = 0.10;    lcuts[lIdx][1][ 1] =  0.20; lcuts[lIdx][2][ 1] =  0.30; //DCAPostToPV 2
    lcuts[lIdx][0][ 2] =  1.2;    lcuts[lIdx][1][ 2] =   1.0; lcuts[lIdx][2][ 2] =   0.8; //DCAV0Daughters 3
    lcuts[lIdx][0][ 3] = 2.00;    lcuts[lIdx][1][ 3] =  3.00; lcuts[lIdx][2][ 3] =   4.0; //V0Radius 4
    lcuts[lIdx][0][ 4] = 0.05;    lcuts[lIdx][1][ 4] =   0.1; lcuts[lIdx][2][ 4] =  0.15; //DCAV0ToPV 5
    lcuts[lIdx][0][ 5] =0.006;    lcuts[lIdx][1][ 5] = 0.005; lcuts[lIdx][2][ 5] = 0.004; //V0Mass 6
    lcuts[lIdx][0][ 6] = 0.05;    lcuts[lIdx][1][ 6] =  0.10; lcuts[lIdx][2][ 6] =  0.15; //DCABachToPV 7
    lcuts[lIdx][0][ 7] = 1.00;    lcuts[lIdx][1][ 7] =   0.6; lcuts[lIdx][2][ 7] =   0.5; //DCACascDaughters 8
    lcuts[lIdx][0][ 8] =  0.6;    lcuts[lIdx][1][ 8] =   1.0; lcuts[lIdx][2][ 8] =  2.50; //CascRadius 9
    lcuts[lIdx][0][ 9] = 14.0;    lcuts[lIdx][1][ 9] =  12.0; lcuts[lIdx][2][ 9] =  10.0; //ProperLifetime 10
    lcuts[lIdx][0][10] = 40.0;    lcuts[lIdx][1][10] =  30.0; lcuts[lIdx][2][10] =  20.0; //ProperLifetimeV0 11
    lcuts[lIdx][0][11] =   80;    lcuts[lIdx][1][11] =    90; lcuts[lIdx][2][11] =   100; //MinimumTrackLength 12
    lcuts[lIdx][0][12] =    5;    lcuts[lIdx][1][12] =     4; lcuts[lIdx][2][12] =     3; //TPCdEdx 13
    lcuts[lIdx][0][13] =  0.0;    lcuts[lIdx][1][13] = 0.008; lcuts[lIdx][2][13] = 0.010; //Competing 14
    lcuts[lIdx][0][14] =  0.8;    lcuts[lIdx][1][14] =   0.6; lcuts[lIdx][2][14] =   0.5; //3D DCA Cascade To PV
    //================================================================================
    
    //================================================================================
    // OMEGAPLUS SELECTIONS
    lIdx = 3; //Master OmegaPlus Index
    //--------------------------------------------------------------------------------
    //                  LOOSE                        CENTRAL                   TIGHT
    lcuts[lIdx][0][ 0] = 0.10;    lcuts[lIdx][1][ 0] =  0.20; lcuts[lIdx][2][ 0] =  0.30; //DCANegToPV 1
    lcuts[lIdx][0][ 1] = 0.10;    lcuts[lIdx][1][ 1] =  0.20; lcuts[lIdx][2][ 1] =  0.30; //DCAPostToPV 2
    lcuts[lIdx][0][ 2] =  1.2;    lcuts[lIdx][1][ 2] =   1.0; lcuts[lIdx][2][ 2] =   0.8; //DCAV0Daughters 3
    lcuts[lIdx][0][ 3] = 2.00;    lcuts[lIdx][1][ 3] =  3.00; lcuts[lIdx][2][ 3] =   4.0; //V0Radius 4
    lcuts[lIdx][0][ 4] = 0.05;    lcuts[lIdx][1][ 4] =   0.1; lcuts[lIdx][2][ 4] =  0.15; //DCAV0ToPV 5
    lcuts[lIdx][0][ 5] =0.006;    lcuts[lIdx][1][ 5] = 0.005; lcuts[lIdx][2][ 5] = 0.004; //V0Mass 6
    lcuts[lIdx][0][ 6] = 0.05;    lcuts[lIdx][1][ 6] =  0.10; lcuts[lIdx][2][ 6] =  0.15; //DCABachToPV 7
    lcuts[lIdx][0][ 7] = 1.00;    lcuts[lIdx][1][ 7] =   0.6; lcuts[lIdx][2][ 7] =   0.5; //DCACascDaughters 8
    lcuts[lIdx][0][ 8] =  0.6;    lcuts[lIdx][1][ 8] =   1.0; lcuts[lIdx][2][ 8] =  2.50; //CascRadius 9
    lcuts[lIdx][0][ 9] = 14.0;    lcuts[lIdx][1][ 9] =  12.0; lcuts[lIdx][2][ 9] =  10.0; //ProperLifetime 10
    lcuts[lIdx][0][10] = 40.0;    lcuts[lIdx][1][10] =  30.0; lcuts[lIdx][2][10] =  20.0; //ProperLifetimeV0 11
    lcuts[lIdx][0][11] =   80;    lcuts[lIdx][1][11] =    90; lcuts[lIdx][2][11] =   100; //MinimumTrackLength 12
    lcuts[lIdx][0][12] =    5;    lcuts[lIdx][1][12] =     4; lcuts[lIdx][2][12] =     3; //TPCdEdx 13
    lcuts[lIdx][0][13] =  0.0;    lcuts[lIdx][1][13] = 0.008; lcuts[lIdx][2][13] = 0.010; //Competing 14
    lcuts[lIdx][0][14] =  0.8;    lcuts[lIdx][1][14] =   0.6; lcuts[lIdx][2][14] =   0.5; //3D DCA Cascade To PV
    //================================================================================
    
    //STEP 3: Creation of output objects
    
    //Just a counter and one array, please. Nothing else needed
    AliCascadeResult *lCascadeResult[600];
    Long_t lN = 0;
    
    //Map to mass hypothesis
    AliCascadeResult::EMassHypo lMassHypo[4];
    lMassHypo[0] = AliCascadeResult::kXiMinus;
    lMassHypo[1] = AliCascadeResult::kXiPlus;
    lMassHypo[2] = AliCascadeResult::kOmegaMinus;
    lMassHypo[3] = AliCascadeResult::kOmegaPlus;
    
    
    //Central results: Stored in indices 0, 1, 2, 3 (careful!)
    for(Int_t i = 0 ; i < 4 ; i ++){
        //Central result, customized binning: the one to use, usually
        lCascadeResult[lN] = new AliCascadeResult( Form("%s_Central",lParticleName[i].Data() ),lMassHypo[i],"",lCentbinnumb,lCentbinlimits, lPtbinnumb,lPtbinlimits);
        
        //This is MC: generate profile for G3/F (if ever needed)
        lCascadeResult[lN] -> InitializeProtonProfile(lPtbinnumb,lPtbinlimits);
        
        //Setters for V0 Cuts
        lCascadeResult[lN]->SetCutDCANegToPV            ( lcuts[i][1][ 0] ) ;
        lCascadeResult[lN]->SetCutDCAPosToPV            ( lcuts[i][1][ 1] ) ;
        lCascadeResult[lN]->SetCutDCAV0Daughters        ( lcuts[i][1][ 2] ) ;
        lCascadeResult[lN]->SetCutV0Radius              ( lcuts[i][1][ 3] ) ;
        //Setters for Cascade Cuts
        lCascadeResult[lN]->SetCutDCAV0ToPV             ( lcuts[i][1][ 4] ) ;
        lCascadeResult[lN]->SetCutV0Mass                ( lcuts[i][1][ 5] ) ;
        lCascadeResult[lN]->SetCutDCABachToPV           ( lcuts[i][1][ 6] ) ;
        lCascadeResult[lN]->SetCutDCACascDaughters      ( lcuts[i][1][ 7] ) ;
        lCascadeResult[lN]->SetCutVarDCACascDau ( TMath::Exp(0.0470076), -0.917006, 0, 1, 0.5 );
        lCascadeResult[lN]->SetCutCascRadius            ( lcuts[i][1][ 8] ) ;
        //Miscellaneous
        lCascadeResult[lN]->SetCutProperLifetime        ( lcuts[i][1][ 9] ) ;
        lCascadeResult[lN]->SetCutMaxV0Lifetime         ( lcuts[i][1][10] ) ;
        lCascadeResult[lN]->SetCutMinTrackLength        ( lcuts[i][1][11] ) ;
        lCascadeResult[lN]->SetCutTPCdEdx               ( lcuts[i][1][12] ) ;
        lCascadeResult[lN]->SetCutXiRejection           ( lcuts[i][1][13] ) ;
        lCascadeResult[lN]->SetCutDCACascadeToPV        ( lcuts[i][1][14] ) ;
        
        //Parametric angle cut initializations
        //V0 cosine of pointing angle
        lCascadeResult[lN]->SetCutV0CosPA               ( 0.95 ) ; //+variable
        lCascadeResult[lN]->SetCutVarV0CosPA            (TMath::Exp(10.853),
                                                         -25.0322,
                                                         TMath::Exp(-0.843948),
                                                         -0.890794,
                                                         0.057553);
        
        //Cascade cosine of pointing angle
        lCascadeResult[lN]->SetCutCascCosPA             ( 0.95 ) ; //+variable
        if(i < 2){
            lCascadeResult[lN]->SetCutVarCascCosPA          (TMath::Exp(4.86664),
                                                             -10.786,
                                                             TMath::Exp(-1.33411),
                                                             -0.729825,
                                                             0.0695724);
        }
        if(i >= 2){
            lCascadeResult[lN]->SetCutVarCascCosPA          (TMath::Exp(   12.8752),
                                                             -21.522,
                                                             TMath::Exp( -1.49906),
                                                             -0.813472,
                                                             0.0480962);
        }
        
        //BB cosine of pointing angle
        lCascadeResult[lN]->SetCutBachBaryonCosPA       ( TMath::Cos(0.04) ) ; //+variable
        lCascadeResult[lN]->SetCutVarBBCosPA            (TMath::Exp(-2.29048),
                                                         -20.2016,
                                                         TMath::Exp(-2.9581),
                                                         -0.649153,
                                                         0.00526455);
        
        //Add result to pool
        lN++;
    }
    if ( lUseFull ) {
        //Central Full results: Stored in indices 4, 5, 6, 7 (careful!)
        for(Int_t i = 0 ; i < 4 ; i ++){
            lCascadeResult[lN] = new AliCascadeResult( Form("%s_Central_Full",lParticleName[i].Data() ),lMassHypo[i]);
            
            //This is MC: generate profile for G3/F (if ever needed)
            lCascadeResult[lN] -> InitializeProtonProfile(lPtbinnumb,lPtbinlimits);
            
            //Setters for V0 Cuts
            lCascadeResult[lN]->SetCutDCANegToPV            ( lcuts[i][1][ 0] ) ;
            lCascadeResult[lN]->SetCutDCAPosToPV            ( lcuts[i][1][ 1] ) ;
            lCascadeResult[lN]->SetCutDCAV0Daughters        ( lcuts[i][1][ 2] ) ;
            lCascadeResult[lN]->SetCutV0Radius              ( lcuts[i][1][ 3] ) ;
            //Setters for Cascade Cuts
            lCascadeResult[lN]->SetCutDCAV0ToPV             ( lcuts[i][1][ 4] ) ;
            lCascadeResult[lN]->SetCutV0Mass                ( lcuts[i][1][ 5] ) ;
            lCascadeResult[lN]->SetCutDCABachToPV           ( lcuts[i][1][ 6] ) ;
            lCascadeResult[lN]->SetCutDCACascDaughters      ( lcuts[i][1][ 7] ) ;
            lCascadeResult[lN]->SetCutVarDCACascDau ( TMath::Exp(0.0470076), -0.917006, 0, 1, 0.5 );
            lCascadeResult[lN]->SetCutCascRadius            ( lcuts[i][1][ 8] ) ;
            //Miscellaneous
            lCascadeResult[lN]->SetCutProperLifetime        ( lcuts[i][1][ 9] ) ;
            lCascadeResult[lN]->SetCutMaxV0Lifetime         ( lcuts[i][1][10] ) ;
            lCascadeResult[lN]->SetCutMinTrackLength        ( lcuts[i][1][11] ) ;
            lCascadeResult[lN]->SetCutTPCdEdx               ( lcuts[i][1][12] ) ;
            lCascadeResult[lN]->SetCutXiRejection           ( lcuts[i][1][13] ) ;
            lCascadeResult[lN]->SetCutDCACascadeToPV        ( lcuts[i][1][14] ) ;
            
            //Parametric angle cut initializations
            //V0 cosine of pointing angle
            lCascadeResult[lN]->SetCutV0CosPA               ( 0.95 ) ; //+variable
            lCascadeResult[lN]->SetCutVarV0CosPA            (TMath::Exp(10.853),
                                                             -25.0322,
                                                             TMath::Exp(-0.843948),
                                                             -0.890794,
                                                             0.057553);
            
            //Cascade cosine of pointing angle
            lCascadeResult[lN]->SetCutCascCosPA             ( 0.95 ) ; //+variable
            if(i < 2){
                lCascadeResult[lN]->SetCutVarCascCosPA          (TMath::Exp(4.86664),
                                                                 -10.786,
                                                                 TMath::Exp(-1.33411),
                                                                 -0.729825,
                                                                 0.0695724);
            }
            if(i >= 2){
                lCascadeResult[lN]->SetCutVarCascCosPA          (TMath::Exp(   12.8752),
                                                                 -21.522,
                                                                 TMath::Exp( -1.49906),
                                                                 -0.813472,
                                                                 0.0480962);
            }
            
            //BB cosine of pointing angle
            lCascadeResult[lN]->SetCutBachBaryonCosPA       ( TMath::Cos(0.04) ) ; //+variable
            lCascadeResult[lN]->SetCutVarBBCosPA            (TMath::Exp(-2.29048),
                                                             -20.2016,
                                                             TMath::Exp(-2.9581),
                                                             -0.649153,
                                                             0.00526455);
            
            //Add result to pool
            lN++;
        }
    }
    
    //Explore restricted rapidity range check
    for(Int_t i = 0 ; i < 4 ; i ++){
        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_Central_y03",lParticleName[i].Data() ) );
        
        lCascadeResult[lN] -> SetCutMinRapidity(-0.3);
        lCascadeResult[lN] -> SetCutMaxRapidity(+0.3);
        
        //Add result to pool
        lN++;
    }
    
    Float_t lLowRap = -0.6;
    Float_t lHighRap = -0.5;
    for(Int_t i=0;i<4;i++){
        lLowRap = -0.6;
        lHighRap = -0.5;
        for(Int_t irapbin=0;irapbin<12;irapbin++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%f_%f",lParticleName[i].Data(),"DefaultRapiditySweep",lLowRap,lHighRap ) );
            lCascadeResult[lN]->SetCutMinRapidity(lLowRap);
            lCascadeResult[lN]->SetCutMaxRapidity(lHighRap);
            lN++;
            lLowRap+=0.1;
            lHighRap+=0.1;
        }
    }
    
    // STEP 4: Creation of objects to be used in systematics
    // Optimized via use of copy constructors
    for(Int_t i = 0 ; i < 4 ; i ++){
        for(Int_t iCut = 0 ; iCut < 15 ; iCut ++){
            
            //LOOSE VARIATIONS
            //Create a new object from default
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),lCutName[iCut].Data(),lConfName[0].Data()) );
            
            if(iCut ==  0 ) lCascadeResult[lN]->SetCutDCANegToPV            ( lcuts[i][0][iCut] ) ;
            if(iCut ==  1 ) lCascadeResult[lN]->SetCutDCAPosToPV            ( lcuts[i][0][iCut] ) ;
            if(iCut ==  2 ) lCascadeResult[lN]->SetCutDCAV0Daughters        ( lcuts[i][0][iCut] ) ;
            if(iCut ==  3 ) lCascadeResult[lN]->SetCutV0Radius              ( lcuts[i][0][iCut] ) ;
            
            //Setters for Cascade Cuts
            if(iCut ==  4 ) lCascadeResult[lN]->SetCutDCAV0ToPV             ( lcuts[i][0][iCut] ) ;
            if(iCut ==  5 ) lCascadeResult[lN]->SetCutV0Mass                ( lcuts[i][0][iCut] ) ;
            if(iCut ==  6 ) lCascadeResult[lN]->SetCutDCABachToPV           ( lcuts[i][0][iCut] ) ;
            if(iCut ==  7 ){
                lCascadeResult[lN]->SetCutDCACascDaughters      ( lcuts[i][0][iCut] ) ;
                lCascadeResult[lN]->SetCutVarDCACascDau ( 1.2 * TMath::Exp(0.0470076), -0.917006, 0, 1, 1.2 * 0.5 );
            }
            if(iCut ==  8 ) lCascadeResult[lN]->SetCutCascRadius            ( lcuts[i][0][iCut] ) ;
            
            //Miscellaneous
            if(iCut ==  9 ) lCascadeResult[lN]->SetCutProperLifetime        ( lcuts[i][0][iCut] ) ;
            if(iCut == 10 ) lCascadeResult[lN]->SetCutMaxV0Lifetime         ( lcuts[i][0][iCut] ) ;
            if(iCut == 11 ) lCascadeResult[lN]->SetCutMinTrackLength        ( lcuts[i][0][iCut] ) ;
            if(iCut == 12 ) lCascadeResult[lN]->SetCutTPCdEdx               ( lcuts[i][0][iCut] ) ;
            if(iCut == 13 ) lCascadeResult[lN]->SetCutXiRejection           ( lcuts[i][0][iCut] ) ;
            if(iCut == 14 ) lCascadeResult[lN]->SetCutDCACascadeToPV        ( lcuts[i][0][iCut] ) ;
            
            //Print this variation, add to pool
            //lCascadeResult[lN]->Print();
            lN++;
            
            //TIGHT VARIATIONS
            //Create a new object from default
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),lCutName[iCut].Data(),lConfName[2].Data()) );
            
            if(iCut ==  0 ) lCascadeResult[lN]->SetCutDCANegToPV            ( lcuts[i][2][iCut] ) ;
            if(iCut ==  1 ) lCascadeResult[lN]->SetCutDCAPosToPV            ( lcuts[i][2][iCut] ) ;
            if(iCut ==  2 ) lCascadeResult[lN]->SetCutDCAV0Daughters        ( lcuts[i][2][iCut] ) ;
            if(iCut ==  3 ) lCascadeResult[lN]->SetCutV0Radius              ( lcuts[i][2][iCut] ) ;
            
            //Setters for Cascade Cuts
            if(iCut ==  4 ) lCascadeResult[lN]->SetCutDCAV0ToPV             ( lcuts[i][2][iCut] ) ;
            if(iCut ==  5 ) lCascadeResult[lN]->SetCutV0Mass                ( lcuts[i][2][iCut] ) ;
            if(iCut ==  6 ) lCascadeResult[lN]->SetCutDCABachToPV           ( lcuts[i][2][iCut] ) ;
            if(iCut ==  7 ){
                lCascadeResult[lN]->SetCutDCACascDaughters      ( lcuts[i][2][iCut] ) ;
                lCascadeResult[lN]->SetCutVarDCACascDau ( 0.8 * TMath::Exp(0.0470076), -0.917006, 0, 1, 0.8 * 0.5 );
            }
            if(iCut ==  8 ) lCascadeResult[lN]->SetCutCascRadius            ( lcuts[i][2][iCut] ) ;
            
            //Miscellaneous
            if(iCut ==  9 ) lCascadeResult[lN]->SetCutProperLifetime        ( lcuts[i][2][iCut] ) ;
            if(iCut == 10 ) lCascadeResult[lN]->SetCutMaxV0Lifetime         ( lcuts[i][2][iCut] ) ;
            if(iCut == 11 ) lCascadeResult[lN]->SetCutMinTrackLength        ( lcuts[i][2][iCut] ) ;
            if(iCut == 12 ) lCascadeResult[lN]->SetCutTPCdEdx               ( lcuts[i][2][iCut] ) ;
            if(iCut == 13 ) lCascadeResult[lN]->SetCutXiRejection           ( lcuts[i][2][iCut] ) ;
            if(iCut == 14 ) lCascadeResult[lN]->SetCutDCACascadeToPV        ( lcuts[i][2][iCut] ) ;
            
            //Print this variation, add to pool
            //lCascadeResult[lN]->Print();
            lN++;
        }
    }
    
    //STEP 5: re-parametrization of cosines for tight and loose variations (done manually)
    for(Int_t i = 0 ; i < 4 ; i ++){
        //======================================================
        //V0CosPA Variations
        //======================================================
        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"V0CosPA","Loose") );
        lCascadeResult[lN]->SetCutVarV0CosPA(TMath::Exp(  -1.77429),
                                             -0.692453,
                                             TMath::Exp( -2.01938),
                                             -0.201574,
                                             0.0776465);
        lN++;
        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"V0CosPA","Tight") );
        lCascadeResult[lN]->SetCutVarV0CosPA(TMath::Exp(  -1.21892),
                                             -41.8521,
                                             TMath::Exp(   -1.278),
                                             -0.894064,
                                             0.0303932);
        lN++;
        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"V0CosPA","VeryTight") );
        lCascadeResult[lN]->SetCutVarV0CosPA(TMath::Exp(   12.8077),
                                             -21.2944,
                                             TMath::Exp( -1.53357),
                                             -0.920017,
                                             0.0262315);
        
        lN++;
        //======================================================
        //CascCosPA Variations
        //======================================================
        if( i < 2 ){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"CascCosPA","Loose") );
            lCascadeResult[lN]->SetCutVarCascCosPA(TMath::Exp(  -1.77429),
                                                   -0.692453,
                                                   TMath::Exp( -2.01938),
                                                   -0.201574,
                                                   0.0776465);
            lN++;
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"CascCosPA","Tight") );
            lCascadeResult[lN]->SetCutVarCascCosPA(TMath::Exp(   12.8752),
                                                   -21.522,
                                                   TMath::Exp( -1.49906),
                                                   -0.813472,
                                                   0.0480962);
            lN++;
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"CascCosPA","VeryTight") );
            lCascadeResult[lN]->SetCutVarCascCosPA(TMath::Exp(    12.801),
                                                   -21.6157,
                                                   TMath::Exp( -1.66297),
                                                   -0.889246,
                                                   0.0346838);
            lN++;
        }
        if( i >= 2 ){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"CascCosPA","Loose") );
            lCascadeResult[lN]->SetCutVarCascCosPA(TMath::Exp(4.86664),
                                                   -10.786,
                                                   TMath::Exp(-1.33411),
                                                   -0.729825,
                                                   0.0695724);
            lN++;
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"CascCosPA","Tight") );
            lCascadeResult[lN]->SetCutVarCascCosPA(TMath::Exp(    12.801),
                                                   -21.6157,
                                                   TMath::Exp( -1.66297),
                                                   -0.889246,
                                                   0.0346838);
            lN++;
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"CascCosPA","VeryTight") );
            lCascadeResult[lN]->SetCutCascCosPA             ( 0.9992 );
            lN++;
        }
        //======================================================
        //BBCosPA Variations
        //======================================================
        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"BBCosPA","Loose") );
        lCascadeResult[lN]->SetCutBachBaryonCosPA        ( TMath::Cos(0.03) ) ;
        lCascadeResult[lN]->SetCutVarBBCosPA(TMath::Exp(    -2.8798),
                                             -20.9876,
                                             TMath::Exp(  -3.10847),
                                             -0.73045,
                                             0.00235147);
        lN++;
        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%s",lParticleName[i].Data(),"BBCosPA","Tight") );
        lCascadeResult[lN]->SetCutBachBaryonCosPA        ( TMath::Cos(0.05) ) ;
        lCascadeResult[lN]->SetCutVarBBCosPA(TMath::Exp(   12.4606),
                                             -20.578,
                                             TMath::Exp( -2.41442),
                                             -0.709588,
                                             0.01079);
        lN++;
    }
    
    //STEP 6: V0 Mass sweep
    //for(Int_t i = 0 ; i < 4 ; i ++){
    //    for(Int_t isweep=0; isweep<20;isweep++){
    //        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_V0MassSweep_%i",lParticleName[i].Data(),isweep) );
    //        lCascadeResult[lN]->SetCutV0MassSigma( ((Double_t)(isweep)/4000.0)); //in GeV/c^2
    //        lN++;
    //    }
    //}
    
    Float_t lLifetimeCut[4];
    lLifetimeCut[0] = 15.0;
    lLifetimeCut[1] = 15.0;
    lLifetimeCut[2] = 12.0;
    lLifetimeCut[3] = 12.0;
    
    Float_t lMass[4];
    lMass[0] = 1.322;
    lMass[1] = 1.322;
    lMass[2] = 1.672;
    lMass[3] = 1.672;
    
    //Old vertexer-level configuration for cross-checks
    for(Int_t i = 0 ; i < 4 ; i ++){
        //Central result, customized binning: the one to use, usually
        lCascadeResult[lN] = new AliCascadeResult( Form("%s_VertexerLevel",lParticleName[i].Data() ),lMassHypo[i],"",lCentbinnumb,lCentbinlimits, lPtbinnumb,lPtbinlimits,100,lMass[i]-0.050,lMass[i]+0.050);
        
        
        //This is MC: generate profile for G3/F (if ever needed)
        lCascadeResult[lN] -> InitializeProtonProfile(lPtbinnumb,lPtbinlimits);
        
        //Default cuts: use vertexer level ones
        //Setters for V0 Cuts
        lCascadeResult[lN]->SetCutDCANegToPV            ( 0.2 ) ;
        lCascadeResult[lN]->SetCutDCAPosToPV            ( 0.2 ) ;
        lCascadeResult[lN]->SetCutDCAV0Daughters        (  1. ) ;
        lCascadeResult[lN]->SetCutV0CosPA               ( 0.98 ) ;
        lCascadeResult[lN]->SetCutV0Radius              (  3 ) ;
        //Setters for Cascade Cuts
        lCascadeResult[lN]->SetCutDCAV0ToPV             ( 0.1 ) ;
        lCascadeResult[lN]->SetCutV0Mass                ( 0.006 ) ;
        lCascadeResult[lN]->SetCutDCABachToPV           ( 0.03 ) ;
        lCascadeResult[lN]->SetCutDCACascDaughters      ( 1. ) ;
        lCascadeResult[lN]->SetCutCascRadius            ( 1.2 ) ;
        if(i==2||i==3)
            lCascadeResult[lN]->SetCutCascRadius            ( 1.0 ) ; //omega case
        lCascadeResult[lN]->SetCutCascCosPA             ( 0.98 ) ;
        //Miscellaneous
        lCascadeResult[lN]->SetCutProperLifetime        ( lLifetimeCut[i] ) ;
        lCascadeResult[lN]->SetCutMinTrackLength           ( 90.0 ) ;
        lCascadeResult[lN]->SetCutTPCdEdx               ( 4.0 ) ;
        lCascadeResult[lN]->SetCutXiRejection           ( 0.008 ) ;
        lCascadeResult[lN]->SetCutBachBaryonCosPA        ( TMath::Cos(0.006) ) ;
        //Add result to pool
        lN++;
    }
    
    for (Int_t iconf = 0; iconf<lN; iconf++){
        cout<<"["<<iconf<<"/"<<lN<<"] Adding config named "<<lCascadeResult[iconf]->GetName()<<endl;
        AddConfiguration(lCascadeResult[iconf]);
    }
    cout<<"Added "<<lN<<" Cascade configurations to output."<<endl;
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityMCRun2::AddCascadeConfiguration276TeV()
//Adds 2.76 TeV cascade analysis configuration
{
    // STEP 1: Decide on binning (needed to improve on memory consumption)
    
    // pT binning
    Double_t lPtbinlimits[] = {0.4, 0.5, 0.6,
        0.7,0.8,.9,1.0,1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,2.0,
        2.1,2.2,2.3,2.4,2.5,2.6,2.7,2.8,3.0,3.2,3.4,3.6,3.8,4.0,4.2,
        4.4,4.6,4.8,5.0,5.5,6.0,6.5,7.0,8.0,9.0,10.,11.,12.};
    Long_t lPtbinnumb = sizeof(lPtbinlimits)/sizeof(Double_t) - 1;
    
    // centrality binning
    Double_t lCentbinlimits[] = {0, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90};
    Long_t lCentbinnumb = sizeof(lCentbinlimits)/sizeof(Double_t) - 1;
    
    // TStrings for output names
    TString lParticleName[] = {"XiMinus", "XiPlus",  "OmegaMinus", "OmegaPlus"};
    
    //Just a counter and one array, please. Nothing else needed
    AliCascadeResult *lCascadeResult[100];
    Long_t lN = 0;
    
    //Map to mass hypothesis
    AliCascadeResult::EMassHypo lMassHypo[4];
    lMassHypo[0] = AliCascadeResult::kXiMinus;
    lMassHypo[1] = AliCascadeResult::kXiPlus;
    lMassHypo[2] = AliCascadeResult::kOmegaMinus;
    lMassHypo[3] = AliCascadeResult::kOmegaPlus;
    
    for(Int_t i = 0 ; i < 4 ; i ++){
        //2.76 Config result, customized binning: the one to use, usually
        lCascadeResult[lN] = new AliCascadeResult( Form("%s_276TeV",lParticleName[i].Data() ),lMassHypo[i],"",lCentbinnumb,lCentbinlimits, lPtbinnumb,lPtbinlimits);
        lCascadeResult[lN] -> InitializeProtonProfile(lPtbinnumb,lPtbinlimits);
        
        //Setters for V0 Cuts
        lCascadeResult[lN]->SetCutDCANegToPV            ( 0.1    ) ;
        lCascadeResult[lN]->SetCutDCAPosToPV            ( 0.1    ) ;
        lCascadeResult[lN]->SetCutDCAV0Daughters        ( 0.8    ) ;
        lCascadeResult[lN]->SetCutV0CosPA               ( 0.95   ) ; // + variable
        lCascadeResult[lN]->SetCutUse276TeVV0CosPA      ( kTRUE  ) ;
        lCascadeResult[lN]->SetCutV0Radius              ( 3.0    ) ;
        //Setters for Cascade Cuts
        lCascadeResult[lN]->SetCutDCAV0ToPV             ( 0.1    ) ;
        lCascadeResult[lN]->SetCutV0Mass                ( 0.005  ) ;
        lCascadeResult[lN]->SetCutDCABachToPV           ( 0.03   ) ;
        lCascadeResult[lN]->SetCutDCACascDaughters      ( 0.3    ) ;
        lCascadeResult[lN]->SetCutCascRadius            ( 1.5    ) ;
        lCascadeResult[lN]->SetCutCascCosPA             ( 0.9992 ) ;
        //Miscellaneous
        lCascadeResult[lN]->SetCutProperLifetime        ( 15.0   ) ;
        lCascadeResult[lN]->SetCutLeastNumberOfClusters ( 70     ) ;
        lCascadeResult[lN]->SetCutTPCdEdx               ( 4      ) ;
        lCascadeResult[lN]->SetCutXiRejection           ( 0.008  ) ;
        lCascadeResult[lN]->SetCutDCABachToBaryon       ( 0      ) ;
        
        if(i > 1){
            lCascadeResult[lN]->SetCutCascRadius            ( 1.0 ) ;
            lCascadeResult[lN]->SetCutProperLifetime        ( 8.0 ) ;
        }
        
        //Add result to pool
        lN++;
    }
    
    //Explore restricted rapidity range check
    for(Int_t i = 0 ; i < 4 ; i ++){
        lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_276TeV_y03",lParticleName[i].Data() ) );
        
        lCascadeResult[lN] -> SetCutMinRapidity(-0.3);
        lCascadeResult[lN] -> SetCutMaxRapidity(+0.3);
        
        //Add result to pool
        lN++;
    }
    
    Float_t lLowRap = -0.6;
    Float_t lHighRap = -0.5;
    for(Int_t i=0;i<4;i++){
        lLowRap = -0.6;
        lHighRap = -0.5;
        for(Int_t irapbin=0;irapbin<12;irapbin++){
            lCascadeResult[lN] = new AliCascadeResult( lCascadeResult[i], Form("%s_%s_%f_%f",lParticleName[i].Data(),"276TeVRapiditySweep",lLowRap,lHighRap ) );
            lCascadeResult[lN]->SetCutMinRapidity(lLowRap);
            lCascadeResult[lN]->SetCutMaxRapidity(lHighRap);
            lN++;
            lLowRap+=0.1;
            lHighRap+=0.1;
        }
    }
    
    for (Int_t iconf = 0; iconf<lN; iconf++){
        cout<<"["<<iconf<<"/"<<lN<<"] Adding config named "<<lCascadeResult[iconf]->GetName()<<endl;
        AddConfiguration(lCascadeResult[iconf]);
    }
    
    cout<<"Added "<<lN<<" cascade configurations to output (corresponding to 2.76 TeV analysis cuts)"<<endl;
}

//________________________________________________________________________
Float_t AliAnalysisTaskStrangenessVsMultiplicityMCRun2::GetDCAz(AliESDtrack *lTrack)
//Encapsulation of DCAz calculation
{
    Float_t b[2];
    Float_t bCov[3];
    lTrack->GetImpactParameters(b,bCov);
    if (bCov[0]<=0 || bCov[2]<=0) {
        AliDebug(1, "Estimated b resolution lower or equal to zero!");
        bCov[0]=0; bCov[2]=0;
    }
    Float_t dcaToVertexXY = b[0];
    Float_t dcaToVertexZ = b[1];
    
    return dcaToVertexZ;
}

//________________________________________________________________________
Float_t AliAnalysisTaskStrangenessVsMultiplicityMCRun2::GetCosPA(AliESDtrack *lPosTrack, AliESDtrack *lNegTrack, AliESDEvent *lEvent)
//Encapsulation of CosPA calculation (warning: considers AliESDtrack clones)
{
    Float_t lCosPA = -1;

    //Get Magnetic field and primary vertex
    Double_t b=lEvent->GetMagneticField();
    const AliESDVertex *vtxT3D=lEvent->GetPrimaryVertex();
    Double_t xPrimaryVertex=vtxT3D->GetX();
    Double_t yPrimaryVertex=vtxT3D->GetY();
    Double_t zPrimaryVertex=vtxT3D->GetZ();
    
    //Copy AliExternalParam for handling
    AliExternalTrackParam nt(*lNegTrack), pt(*lPosTrack), *lNegClone=&nt, *lPosClone=&pt;
    
    //Find DCA
    Double_t xn, xp, dca=lNegClone->GetDCA(lPosClone,b,xn,xp);
    
    //Propagate to it
    nt.PropagateTo(xn,b); pt.PropagateTo(xp,b);
    
    //Create V0 object to do propagation
    AliESDv0 vertex(nt,1,pt,2); //Never mind indices, won't use
    
    //Get CosPA
    lCosPA=vertex.GetV0CosineOfPointingAngle(xPrimaryVertex,yPrimaryVertex,zPrimaryVertex);
    
    //Return value
    return lCosPA;
}


//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityMCRun2::CheckChargeV0(AliESDv0 *v0)
{
    // This function checks charge of negative and positive daughter tracks.
    // If incorrectly defined (onfly vertexer), swaps out.
    if( v0->GetParamN()->Charge() > 0 && v0->GetParamP()->Charge() < 0 ) {
        //V0 daughter track swapping is required! Note: everything is swapped here... P->N, N->P
        Long_t lCorrectNidx = v0->GetPindex();
        Long_t lCorrectPidx = v0->GetNindex();
        Double32_t	lCorrectNmom[3];
        Double32_t	lCorrectPmom[3];
        v0->GetPPxPyPz( lCorrectNmom[0], lCorrectNmom[1], lCorrectNmom[2] );
        v0->GetNPxPyPz( lCorrectPmom[0], lCorrectPmom[1], lCorrectPmom[2] );
        
        AliExternalTrackParam	lCorrectParamN(
                                               v0->GetParamP()->GetX() ,
                                               v0->GetParamP()->GetAlpha() ,
                                               v0->GetParamP()->GetParameter() ,
                                               v0->GetParamP()->GetCovariance()
                                               );
        AliExternalTrackParam	lCorrectParamP(
                                               v0->GetParamN()->GetX() ,
                                               v0->GetParamN()->GetAlpha() ,
                                               v0->GetParamN()->GetParameter() ,
                                               v0->GetParamN()->GetCovariance()
                                               );
        lCorrectParamN.SetMostProbablePt( v0->GetParamP()->GetMostProbablePt() );
        lCorrectParamP.SetMostProbablePt( v0->GetParamN()->GetMostProbablePt() );
        
        //Get Variables___________________________________________________
        Double_t lDcaV0Daughters = v0 -> GetDcaV0Daughters();
        Double_t lCosPALocal     = v0 -> GetV0CosineOfPointingAngle();
        Bool_t lOnFlyStatusLocal = v0 -> GetOnFlyStatus();
        
        //Create Replacement Object_______________________________________
        AliESDv0 *v0correct = new AliESDv0(lCorrectParamN,lCorrectNidx,lCorrectParamP,lCorrectPidx);
        v0correct->SetDcaV0Daughters          ( lDcaV0Daughters   );
        v0correct->SetV0CosineOfPointingAngle ( lCosPALocal       );
        v0correct->ChangeMassHypothesis       ( kK0Short          );
        v0correct->SetOnFlyStatus             ( lOnFlyStatusLocal );
        
        //Reverse Cluster info..._________________________________________
        v0correct->SetClusters( v0->GetClusters( 1 ), v0->GetClusters ( 0 ) );
        
        *v0 = *v0correct;
        //Proper cleanup..._______________________________________________
        v0correct->Delete();
        v0correct = 0x0;
        
        //Just another cross-check and output_____________________________
        if( v0->GetParamN()->Charge() > 0 && v0->GetParamP()->Charge() < 0 ) {
            AliWarning("Found Swapped Charges, tried to correct but something FAILED!");
        } else {
            //AliWarning("Found Swapped Charges and fixed.");
        }
        //________________________________________________________________
    } else {
        //Don't touch it! ---
        //Printf("Ah, nice. Charges are already ordered...");
    }
    return;
}

//______________________________________________________________________
AliAnalysisTaskStrangenessVsMultiplicityMCRun2::FMDhits AliAnalysisTaskStrangenessVsMultiplicityMCRun2::GetFMDhits(AliAODEvent* aodEvent) const
// Relies on the event being vaild (no extra checks if object exists done here)
{
    AliAODForwardMult* aodForward = static_cast<AliAODForwardMult*>(aodEvent->FindListObject("Forward"));
    // Shape of d2Ndetadphi: 200, -4, 6, 20, 0, 2pi
    const TH2D& d2Ndetadphi = aodForward->GetHistogram();
    Int_t nEta = d2Ndetadphi.GetXaxis()->GetNbins();
    Int_t nPhi = d2Ndetadphi.GetYaxis()->GetNbins();
    FMDhits ret_vector;
    for (Int_t iEta = 1; iEta <= nEta; iEta++) {
        Int_t valid = Int_t(d2Ndetadphi.GetBinContent(iEta, 0));
        if (!valid) {
            // No data expected for this eta
            continue;
        }
        Float_t eta = d2Ndetadphi.GetXaxis()->GetBinCenter(iEta);
        for (Int_t iPhi = 1; iPhi <= nPhi; iPhi++) {
            // Bin content is most likely number of particles!
            Float_t mostProbableN = d2Ndetadphi.GetBinContent(iEta, iPhi);
            if (mostProbableN > 0) {
                Float_t phi = d2Ndetadphi.GetYaxis()->GetBinCenter(iPhi);
                ret_vector.push_back(AliAnalysisTaskStrangenessVsMultiplicityMCRun2::FMDhit(eta, phi, mostProbableN));
            }
        }
    }
    return ret_vector;
}

//________________________________________________________________________
Double_t AliAnalysisTaskStrangenessVsMultiplicityMCRun2::Det(Double_t a00, Double_t a01, Double_t a10, Double_t a11) const {
    //--------------------------------------------------------------------
    // This function calculates locally a 2x2 determinant
    //--------------------------------------------------------------------
    return a00*a11 - a01*a10;
}

//________________________________________________________________________
Double_t AliAnalysisTaskStrangenessVsMultiplicityMCRun2::Det(Double_t a00,Double_t a01,Double_t a02,
                                                             Double_t a10,Double_t a11,Double_t a12,
                                                             Double_t a20,Double_t a21,Double_t a22) const {
    //--------------------------------------------------------------------
    // This function calculates locally a 3x3 determinant
    //--------------------------------------------------------------------
    return  a00*Det(a11,a12,a21,a22)-a01*Det(a10,a12,a20,a22)+a02*Det(a10,a11,a20,a21);
}

//________________________________________________________________________
Double_t AliAnalysisTaskStrangenessVsMultiplicityMCRun2::PropagateToDCA(AliESDv0 *v, AliExternalTrackParam *t, AliESDEvent *event, Double_t b) {
    //--------------------------------------------------------------------
    // This function returns the DCA between the V0 and the track
    //--------------------------------------------------------------------
    Double_t alpha=t->GetAlpha(), cs1=TMath::Cos(alpha), sn1=TMath::Sin(alpha);
    Double_t r[3]; t->GetXYZ(r);
    Double_t x1=r[0], y1=r[1], z1=r[2];
    Double_t p[3]; t->GetPxPyPz(p);
    Double_t px1=p[0], py1=p[1], pz1=p[2];
    
    Double_t x2,y2,z2;     // position and momentum of V0
    Double_t px2,py2,pz2;
    
    v->GetXYZ(x2,y2,z2);
    v->GetPxPyPz(px2,py2,pz2);
    
    Double_t dca = 1e+33;
    if ( !fkDoImprovedCascadeVertexFinding || fkIfImprovedPerformInitialLinearPropag ){
        // calculation dca
        Double_t dd= Det(x2-x1,y2-y1,z2-z1,px1,py1,pz1,px2,py2,pz2);
        Double_t ax= Det(py1,pz1,py2,pz2);
        Double_t ay=-Det(px1,pz1,px2,pz2);
        Double_t az= Det(px1,py1,px2,py2);
        
        dca=TMath::Abs(dd)/TMath::Sqrt(ax*ax + ay*ay + az*az);
        
        //points of the DCA
        Double_t t1 = Det(x2-x1,y2-y1,z2-z1,px2,py2,pz2,ax,ay,az)/
        Det(px1,py1,pz1,px2,py2,pz2,ax,ay,az);
        
        x1 += px1*t1; y1 += py1*t1; //z1 += pz1*t1;
        
        //propagate track to the points of DCA
        
        x1=x1*cs1 + y1*sn1;
        if (!t->PropagateTo(x1,b)) {
            Error("PropagateToDCA","Propagation failed !");
            return 1.e+33;
        }
    }
    
    if( fkDoImprovedCascadeVertexFinding ){
        //DCA Calculation improved -> non-linear propagation
        //Preparatory step 1: get two tracks corresponding to V0
        UInt_t lKeyPos = (UInt_t)TMath::Abs(v->GetPindex());
        UInt_t lKeyNeg = (UInt_t)TMath::Abs(v->GetNindex());
        AliESDtrack *pTrack=((AliESDEvent*)event)->GetTrack(lKeyPos);
        AliESDtrack *nTrack=((AliESDEvent*)event)->GetTrack(lKeyNeg);
        
        //Uncertainties: bachelor track as well as V0
        Double_t dy2=t->GetSigmaY2() + pTrack->GetSigmaY2() + nTrack->GetSigmaY2();
        Double_t dz2=t->GetSigmaZ2() + pTrack->GetSigmaZ2() + nTrack->GetSigmaZ2();
        Double_t dx2=dy2;
        
        if( TMath::Abs(fkIfImprovedExtraPrecisionFactor-1.0)>1e-4 ){
            //For testing purposes: override uncertainties, please
            dx2 = fkIfImprovedExtraPrecisionFactor;
            dy2 = fkIfImprovedExtraPrecisionFactor;
            dz2 = fkIfImprovedExtraPrecisionFactor;
        }
        
        //Create dummy V0 track
        //V0 properties to get started
        Double_t xyz[3], pxpypz[3], cv[21];
        for(Int_t ii=0;ii<21;ii++) cv[ii]=0.0; //something small
        
        v->GetXYZ(xyz[0],xyz[1],xyz[2]);
        v->GetPxPyPz( pxpypz[0],pxpypz[1],pxpypz[2] );
        
        //Mockup track for V0 trajectory (no covariance)
        //AliExternalTrackParam *hV0Traj = new AliExternalTrackParam(xyz,pxpypz,cv,+1);
        AliExternalTrackParam lV0TrajObject(xyz,pxpypz,cv,+1), *hV0Traj = &lV0TrajObject;
        hV0Traj->ResetCovariance(1); //won't use
        
        Double_t p1[8]; t->GetHelixParameters(p1,b);
        p1[6]=TMath::Sin(p1[2]); p1[7]=TMath::Cos(p1[2]);
        Double_t p2[8]; hV0Traj->GetHelixParameters(p2,0.0); //p2[4]=0 -> no curvature (fine, predicted in Evaluate)
        p2[6]=TMath::Sin(p2[2]); p2[7]=TMath::Cos(p2[2]);
        
        Double_t r1[3],g1[3],gg1[3]; Double_t t1=0.;
        Evaluate(p1,t1,r1,g1,gg1);
        Double_t r2[3],g2[3],gg2[3]; Double_t t2=0.;
        Evaluate(p2,t2,r2,g2,gg2);
        
        Double_t dx=r2[0]-r1[0], dy=r2[1]-r1[1], dz=r2[2]-r1[2];
        Double_t dm=dx*dx/dx2 + dy*dy/dy2 + dz*dz/dz2;
        
        Int_t max=27;
        while (max--) {
            Double_t gt1=-(dx*g1[0]/dx2 + dy*g1[1]/dy2 + dz*g1[2]/dz2);
            Double_t gt2=+(dx*g2[0]/dx2 + dy*g2[1]/dy2 + dz*g2[2]/dz2);
            Double_t h11=(g1[0]*g1[0] - dx*gg1[0])/dx2 +
            (g1[1]*g1[1] - dy*gg1[1])/dy2 +
            (g1[2]*g1[2] - dz*gg1[2])/dz2;
            Double_t h22=(g2[0]*g2[0] + dx*gg2[0])/dx2 +
            (g2[1]*g2[1] + dy*gg2[1])/dy2 +
            (g2[2]*g2[2] + dz*gg2[2])/dz2;
            Double_t h12=-(g1[0]*g2[0]/dx2 + g1[1]*g2[1]/dy2 + g1[2]*g2[2]/dz2);
            
            Double_t det=h11*h22-h12*h12;
            
            Double_t dt1,dt2;
            if (TMath::Abs(det)<1.e-33) {
                //(quasi)singular Hessian
                dt1=-gt1; dt2=-gt2;
            } else {
                dt1=-(gt1*h22 - gt2*h12)/det;
                dt2=-(h11*gt2 - h12*gt1)/det;
            }
            
            if ((dt1*gt1+dt2*gt2)>0) {dt1=-dt1; dt2=-dt2;}
            
            //check delta(phase1) ?
            //check delta(phase2) ?
            
            if (TMath::Abs(dt1)/(TMath::Abs(t1)+1.e-3) < 1.e-4)
                if (TMath::Abs(dt2)/(TMath::Abs(t2)+1.e-3) < 1.e-4) {
                    if ((gt1*gt1+gt2*gt2) > 1.e-4/dy2/dy2)
                        AliDebug(1," stopped at not a stationary point !");
                    Double_t lmb=h11+h22; lmb=lmb-TMath::Sqrt(lmb*lmb-4*det);
                    if (lmb < 0.)
                        AliDebug(1," stopped at not a minimum !");
                    break;
                }
            
            Double_t dd=dm;
            for (Int_t div=1 ; ; div*=2) {
                Evaluate(p1,t1+dt1,r1,g1,gg1);
                Evaluate(p2,t2+dt2,r2,g2,gg2);
                dx=r2[0]-r1[0]; dy=r2[1]-r1[1]; dz=r2[2]-r1[2];
                dd=dx*dx/dx2 + dy*dy/dy2 + dz*dz/dz2;
                if (dd<dm) break;
                dt1*=0.5; dt2*=0.5;
                if (div>512) {
                    AliDebug(1," overshoot !"); break;
                }
            }
            dm=dd;
            
            t1+=dt1;
            t2+=dt2;
            
        }
        
        if (max<=0) AliDebug(1," too many iterations !");
        
        Double_t cs=TMath::Cos(t->GetAlpha());
        Double_t sn=TMath::Sin(t->GetAlpha());
        Double_t xthis=r1[0]*cs + r1[1]*sn;
        
        //Memory cleanup
        hV0Traj->Delete();
        hV0Traj=0x0;
        
        //Propagate bachelor to the point of DCA
        if (!t->PropagateTo(xthis,b)) {
            //AliWarning(" propagation failed !";
            return 1e+33;
        }
        
        
        //V0 distance to bachelor: the desired distance
        Double_t rBachDCAPt[3]; t->GetXYZ(rBachDCAPt);
        dca = v->GetD(rBachDCAPt[0],rBachDCAPt[1],rBachDCAPt[2]);
    }
    
    return dca;
}

//________________________________________________________________________
void AliAnalysisTaskStrangenessVsMultiplicityMCRun2::Evaluate(const Double_t *h, Double_t t,
                                                              Double_t r[3],  //radius vector
                                                              Double_t g[3],  //first defivatives
                                                              Double_t gg[3]) //second derivatives
{
    //--------------------------------------------------------------------
    // Calculate position of a point on a track and some derivatives
    //--------------------------------------------------------------------
    Double_t phase=h[4]*t+h[2];
    Double_t sn=TMath::Sin(phase), cs=TMath::Cos(phase);
    
    r[0] = h[5];
    r[1] = h[0];
    if (TMath::Abs(h[4])>kAlmost0) {
        r[0] += (sn - h[6])/h[4];
        r[1] -= (cs - h[7])/h[4];
    } else {
        r[0] += t*cs;
        r[1] -= -t*sn;
    }
    r[2] = h[1] + h[3]*t;
    
    g[0] = cs; g[1]=sn; g[2]=h[3];
    
    gg[0]=-h[4]*sn; gg[1]=h[4]*cs; gg[2]=0.;
}

//________________________________________________________________________
Double_t AliAnalysisTaskStrangenessVsMultiplicityMCRun2::GetErrorInPosition(AliExternalTrackParam *t1) const {
    Double_t alpha=t1->GetAlpha(), cs=TMath::Cos(alpha), sn=TMath::Sin(alpha);
    Double_t tmp[3];
    t1->GetPxPyPz(tmp);
    Double_t px1=tmp[0], py1=tmp[1], pz1=tmp[2];
    t1->GetXYZ(tmp);
    Double_t  x1=tmp[0],  y1=tmp[1],  z1=tmp[2];
    const Double_t ss=0.0005*0.0005;//a kind of a residual misalignment precision
    Double_t sx1=sn*sn*t1->GetSigmaY2()+ss, sy1=cs*cs*t1->GetSigmaY2()+ss;
    return sx1;
}
