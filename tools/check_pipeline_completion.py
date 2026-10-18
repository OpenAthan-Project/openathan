#!/usr/bin/env python3
"""Run the production pipeline state machine with deterministic platform I/O."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'firmware/esphome/components/speaker/media_player/audio_pipeline.cpp').read_text()
header = (ROOT / 'firmware/esphome/components/speaker/media_player/audio_pipeline.h').read_text()
def extract(text, signature):
    match = re.search(r'^' + re.escape(signature) + r' \{.*?^\}', text, re.M | re.S)
    if not match:
        raise ValueError('Missing production body: ' + signature)
    return match[0]
with tempfile.TemporaryDirectory(prefix='openathan-pipeline-') as temporary:
    work = Path(temporary)
    # PipelineOutcome is a one-line enum; extract it separately to avoid a
    # multiline expression consuming the following declaration.
    types = re.search(r'enum class PipelineOutcome[^;]+;', header)[0] + '\n' + '\n'.join(
        extract(header, signature) + ';' for signature in ('enum class AudioPipelineState : uint8_t',
        'enum class InfoErrorSource : uint8_t', 'enum class DecodingError : uint8_t', 'struct InfoErrorEvent'))
    types = types.replace('optional<', 'std::optional<')
    types += '\n' + extract(source, 'enum EventGroupBits : uint32_t') + ';\n'
    (work / 'pipeline_types.inc').write_text(types)
    (work / 'pipeline_methods.inc').write_text('\n'.join(extract(source, signature) for signature in (
        'void AudioPipeline::start_file(audio::AudioFile *audio_file)', 'void AudioPipeline::start_url(const std::string &uri)',
        'esp_err_t AudioPipeline::stop()', 'AudioPipelineState AudioPipeline::process_state()')))
    output = work / 'pipeline'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                    '-I' + str(work), str(ROOT / 'tests/pipeline_completion_tests.cpp'), '-o', str(output)], check=True)
    subprocess.run([str(output)], check=True)
