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
}

configs = [
    {
        'name': 'engine',
        'path': f'{DATA_DIR}/engine.vtu',
        'opts': {
            '--step_size': '0.2',
            '--transfer_function': f'{TF_DIR}/engine.xf',
            '--camera': '7.81741 174.033 -196.914 8.26184 173.833 -196.041 0 1 -0',
        },
    },
    {
        'name': 'engine_uniform',
        'path': f'{DATA_DIR}/engine_uniform.vtu',
        'opts': {
            '--step_size': '0.2',
            '--transfer_function': f'{TF_DIR}/engine.xf',
            '--camera': '7.81741 174.033 -196.914 8.26184 173.833 -196.041 0 1 -0',
        }
    },
]



def build_command(program, config, out_dir, render_mode, frame_count, level, flags, options):
    options = {**config['opts'], **options}
    filename = Path(out_dir)/f"{config['name']}_{'marcher' if render_mode == 'raymarcher' else 'woodcock'}_{level}.png"
    options['--save'] = filename
    options['--render_mode'] = render_mode
    options['--benchmark_count'] = frame_count
    command = [program, *flags]
    for k,v in options.items():
        command.append(k)
        command.append(v)
    command.append(config['path'])
    return command


def run_benchmark(program_path, out_dir, render_mode, frame_count, max_level, flags=None, options=None):
    if flags is None:
        flags = common_flags
    if options is None:
        options = common_options

    Path(out_dir).mkdir(parents=True, exist_ok=True)
    results_file = Path(out_dir) / 'results.txt'

    with open(results_file, 'w') as f:
        for config in configs:
            for i in range(max_level + 1):
                tmp_options = {**options, '--coarse_mesh_level': str(i)}

                print(f"Running config {config['name']}")
                cmd = build_command(program_path, config, out_dir, render_mode, frame_count, i, flags=flags, options=tmp_options)
                print(" ".join([str(x) for x in cmd]))
                result = subprocess.run(cmd, capture_output=True, text=True)
                print("Done")

                f.write(f"{config['name'].upper()} uniform max level {i}:\n")
                f.write(result.stdout)

                if result.stderr:
                    f.write(f"\nstderr:\n")
                    f.write(result.stderr)

                f.write('\n\n')
                f.flush()
                # break

if __name__ == '__main__':
    benchmark_frame_count = str(1000)
    max_level = 5

    # Barycentric traversal, vertex samples, no macrocell
    run_benchmark(
        '../build/tet_amr_volume_render_b',
        'tet_amr_bvh_levels_raymarcher',
        'raymarcher',
        benchmark_frame_count,
        max_level
    )

    run_benchmark(
        '../build/tet_amr_volume_render_b',
        'tet_amr_bvh_levels_woodcock',
        'woodcock',
        benchmark_frame_count,
        max_level
    )

    # Barycentric traversal, vertex samples, with macrocell enabled
    run_benchmark(
        '../build/tet_amr_volume_render_b_m',
        'tet_amr_bvh_levels_macrocell_raymarcher',
        'raymarcher',
        benchmark_frame_count,
        max_level
    )

    run_benchmark(
        '../build/tet_amr_volume_render_b_m',
        'tet_amr_bvh_levels_macrocell_woodcock',
        'woodcock',
        benchmark_frame_count,
        max_level
    )
