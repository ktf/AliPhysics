/*! \page READMEoadb Accessing the EMCAL OADB from EOS

The EMCal OADB files have been moved to the EOS directory /eos/experiment/alice/analysis-data/OADB/EMCAL which is accessible via lxplus.
Analysis tasks can access these files via, for example: ```AliDataFile::GetFileNameOADB("EMCAL/EMCALTimeL1PhaseCalib.root")``` which has been implemented into the Tender and the Correction Framework as well as most analysis tasks in AliPhysics.
If you want to have the OADB files locally, you can download them from lxplus via:
~~~{.sh}
rsync -av --delete cern_user@lxplus.cern.ch:/eos/experiment/alice/analysis-data/ /path/to/my/local/oadb/
~~~

In order for local tests to work properly, please add the OADB_PATH global variable to your bashrc or similar.
~~~{.sh}
OADB_PATH=/path/to/my/local/oadb/OADB
~~~
It is crucial, that the additional /OADB is added to the global variable OADB_PATH. This is necessary as the downloaded directory from EOS contains the subfolder OADB. OADB_PATH must point to this subfolder in order to be able to properly load the files.

Furthermore, the "export" command should be used in addition to adding the path to the bashrc. This will make the variable available to all processes:
~~~{.sh}
export OADB_PATH=/path/to/my/local/oadb/OADB
~~~

In addition, a short history of changes to the files in EOS will be listed here:

- 20180213: Moved all files to EOS.
- 20180220: Update of EMCALBadChannels.root with LHC17o BC maps, see https://alice.its.cern.ch/jira/browse/EMCAL-60
- 20180305: Update of EMCALBadChannels.root with LHC17m BC maps, see https://alice.its.cern.ch/jira/browse/EMCAL-105
- 20180308: Update of EMCALBadChannels.root with LHC17ghjk BC maps, see https://alice.its.cern.ch/jira/browse/EMCAL-130 , https://alice.its.cern.ch/jira/browse/EMCAL-65 , https://alice.its.cern.ch/jira/browse/EMCAL-90 , https://alice.its.cern.ch/jira/browse/EMCAL-95

*/
