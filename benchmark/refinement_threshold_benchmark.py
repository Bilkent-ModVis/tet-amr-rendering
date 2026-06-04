import subprocess
from pathlib import Path

DATA_DIR = '../data'
TF_DIR = '../data/tf'

common_flags = [
    '--vertex_data',
    '--offline',
]
common_options = {
    '--field': 'Element Scalar',
    '--fovy': '48.7001',
    '--step_size': '0.2',
    '--camera': '127.5 127.5 -223.134 127.5 127.5 63.5 0 1 -0',
    '--transfer_function': f'{TF_DIR}/engine_refinement.xf',
    '--non_benchmark_woodcock_count': '100',
}

configs = [
    {
        'name': 'engine',
        'path': f'{DATA_DIR}/engine.vtu',
        'opts': {},
        'flags': ['--refinement_threshold_benchmark', '--warmup']
    },
    {
        'name': 'engine_refinement_0',
        'path': f'{DATA_DIR}/engine.vtu',
        'opts': {
            '--refinement_criteria': '0',
            '--render_resolution': '2560 1371',
        },
    },
    {
        'name': 'engine_refinement_128',
        'path': f'{DATA_DIR}/engine.vtu',
        'opts': {
            '--refinement_criteria': '128',
            '--render_resolution': '2560 1371',
        },
    },
    {
        'name': 'engine_refinement_256',
        'path': f'{DATA_DIR}/engine.vtu',
        'opts': {
            '--refinement_criteria': '256',
            '--render_resolution': '2560 1371',
        },
    },
]



def build_command(program, config, out_dir, render_mode, frame_count, flags, options):
    opts = {**config['opts'], **options}
    filename = Path(out_dir)/f"{config['name']}_{'marcher' if render_mode == 'raymarcher' else 'woodcock'}.png"
    opts['--save'] = filename
    opts['--render_mode'] = render_mode
    opts['--benchmark_count'] = frame_count
    command = [program, *flags]
    for k,v in opts.items():
        command.append(k)
        command.append(v)
    command.append(config['path'])
    return command


def run_benchmark(program_path, out_dir, render_mode, frame_count, flags=None, options=None, unstructured=False):
    if flags is None:
        flags = common_flags
    if options is None:
        options = common_options

    Path(out_dir).mkdir(parents=True, exist_ok=True)
    results_file = Path(out_dir) / 'results.txt'

    with open(results_file, 'w') as f:
        for config in configs:
            tmp_flags = flags
            if 'flags' in config:
                tmp_flags = [*flags, *config['flags']]

            print(f"Running config {config['name']}")
            cmd = build_command(program_path, config, out_dir, render_mode, frame_count, flags=tmp_flags, options=options)
            print(" ".join([str(x) for x in cmd]))
            result = subprocess.run(cmd, capture_output=True, text=True)
            print("Done")

            f.write(f"{config['name'].upper()}:\n")
            f.write(result.stdout)

            if result.stderr:
                f.write(f"\nstderr:\n")
                f.write(result.stderr)

            f.write('\n\n')
            f.flush()
            # break

if __name__ == '__main__':
    benchmark_frame_count = str(100)

    # Barycentric traversal, vertex samples, no macrocell
    run_benchmark(
        '../build/tet_amr_volume_render_b',
        'tet_amr_refinement_threshold_raymarcher',
        'raymarcher',
        benchmark_frame_count
    )

    run_benchmark(
        '../build/tet_amr_volume_render_b',
        'tet_amr_refinement_threshold_woodcock',
        'woodcock',
        benchmark_frame_count
    )
