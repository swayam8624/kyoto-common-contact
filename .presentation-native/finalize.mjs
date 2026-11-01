import path from 'node:path';
import { finalizePresentation } from '/Users/swayamsingal/.codex/plugins/cache/openai-primary-runtime/presentations/26.1026.10807/skills/presentations/container_tools/artifact_tool_utils.mjs';
const root='/Users/swayamsingal/Desktop/Programming/kyoto-common-contact';
const skill='/Users/swayamsingal/.codex/plugins/cache/openai-primary-runtime/presentations/26.1026.10807/skills/presentations';
console.log(await finalizePresentation({
 workspaceDir:root,
 candidatePath:root+'/.presentation-native/candidate.pptx',
 finalPath:root+'/output/Common_Broad_Phase_Kyoto_Native_v3.pptx',
 pythonExecutable:'/Users/swayamsingal/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3',
 integrityValidatorPath:skill+'/container_tools/inspect_presentation_package_integrity.py',
 layoutValidatorPath:skill+'/container_tools/inspect_presentation_layout_geometry.py',
 layoutArgs:['--expected-slide-size-emu','12192000,6858000','--validate-heading-fit'],
 explicitTotalSlideCount:12,
 requiredNativeTableOwnerSlides:[],
 fontPolicy:{basis:'design',families:['Arial']},
 verifyArtifactToolImport:true,
 receiptPath:root+'/.presentation-native/validation-v3.json'
}));
