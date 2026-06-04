import subprocess
from pathlib import Path

DATA_DIR = '../data'
TF_DIR = '../data/tf'

common_flags = [
    '--vertex_data',
    '--offline',
    '--benchmark',
    '--warmup',
]
common_options = {
    '--field': 'Element Scalar',
    '--fovy': '48.7001',
    '--step_size': '0.364',
    '--camera': '-15.0823 145.692 -177.637 -14.5901 145.643 -176.768 0 1 -0',
}

configs = [
    {
        'name': 'mri_0',
        'path': f'{DATA_DIR}/mri.vtu',
        'opts': {
            '--transfer_function': f'{TF_DIR}/mri_steps/mri_0.xf',
        }
    },
    {
        'name': 'mri_1',
        'path': f'{DATA_DIR}/mri.vtu',
        'opts': {
            '--transfer_function': f'{TF_DIR}/mri_steps/mri_1.xf',
        }
    },
    {
        'name': 'mri_2',
        'path': f'{DATA_DIR}/mri.vtu',
        'opts': {
            '--transfer_function': f'{TF_DIR}/mri_steps/mri_2.xf',
        }
    },
    {
        'name': 'mri_3',
        'path': f'{DATA_DIR}/mri.vtu',
        'opts': {
            '--transfer_function': f'{TF_DIR}/mri_steps/mri_3.xf',
        }
    },
    {
        'name': 'mri_4',
        'path': f'{DATA_DIR}/mri.vtu',
        'opts': {
            '--transfer_function': f'{TF_DIR}/mri_steps/mri_4.xf',
        }
    },
    {
        'name': 'mri_5',
        'path': f'{DATA_DIR}/mri.vtu',
        'opts': {
            '--transfer_function': f'{TF_DIR}/mri_steps/mri_5.xf',
        }
    },
    {
        'name': 'mri_6',
        'path': f'{DATA_DIR}/mri.vtu',
        'opts': {
            '--transfer_function': f'{TF_DIR}/mri_steps/mri_6.xf',
        }
    },
]



def build_command(program, config, out_dir, render_mode, frame_count, flags, options):
    options = {**config['opts'], **options}
    filename = Path(out_dir)/f"{config['name']}_{'marcher' if render_mode == 'raymarcher' else 'woodcock'}.png"
    options['--save'] = filename
    options['--render_mode'] = render_mode
    options['--benchmark_count'] = frame_count
    command = [program, *flags]
    for k,v in options.items():
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
            if unstructured and 'unstructured_renderer_flags' in config:
                tmp_flags = [*flags, *config['unstructured_renderer_flags']]

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
    benchmark_frame_count = str(1000)

    # Barycentric traversal, vertex samples, no macrocell
    run_benchmark(
        '../build/tet_amr_volume_render_b',
        'tet_amr_mri_tf_opacity_steps_raymarcher',
        'raymarcher',
        benchmark_frame_count
    )