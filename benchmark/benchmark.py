import re
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
        'unstructured_renderer_flags': ['--split_bvh']
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
    {
        'name': 'heptane',
        'path': f'{DATA_DIR}/heptane.vtu',
        'opts': {
            '--step_size': '0.250',
            '--transfer_function': f'{TF_DIR}/heptane.xf',
            '--camera': '224.678 223.326 509.875 224.295 223.138 508.971 0 1 -0',
        }
    },
    {
        'name': 'agulhas',
        'path': f'{DATA_DIR}/agulhas.vtu',
        'opts': {
            '--step_size': '0.1',
            '--transfer_function': f'{TF_DIR}/agulhas.xf',
            '--camera': '38.531 -51.2945 71.2747 38.5295 -51.2636 70.2751 0 1 -0',
        }
    },
    {
        'name': 'advection',
        'path': f'{DATA_DIR}/advection.vtu',
        'opts': {
            '--step_size': '0.1',
            '--transfer_function': f'{TF_DIR}/advection.xf',
            '--camera': '-130.198 90.7135 -691.369 -130.021 90.5447 -690.399 0 1 -0',
            '--scale': '100',
        }
    },
    {
        'name': 'mri',
        'path': f'{DATA_DIR}/mri.vtu',
        'opts': {
            '--step_size': '0.364',
            '--transfer_function': f'{TF_DIR}/mri.xf',
            '--camera': '-15.0823 145.692 -177.637 -14.5901 145.643 -176.768 0 1 -0',
        }
    },
    {
        'name': 'torus',
        'path': f'{DATA_DIR}/torus.vtu',
        'opts': {
            '--step_size': '0.442',
            '--transfer_function': f'{TF_DIR}/torus.xf',
            '--camera': '219.103 208.667 -81.0331 218.703 208.291 -80.197 0 1 -0',
        }
    },
    {
        'name': 'carp',
        'path': f'{DATA_DIR}/carp.vtu',
        'opts': {
            '--step_size': '0.305',
            '--transfer_function': f'{TF_DIR}/carp.xf',
            '--camera': '469.052 155.432 77.442 468.092 155.366 77.7112 0 1 -0',
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
    log_file = Path(out_dir) / 'log.txt'

    times_ms = []
    times_us = []

    with open(log_file, 'w') as f:
        for config in configs:
            tmp_flags = flags
            if unstructured and 'unstructured_renderer_flags' in config:
                tmp_flags = [*flags, *config['unstructured_renderer_flags']]

            print(f"Running config {config['name']}")
            cmd = build_command(program_path, config, out_dir, render_mode, frame_count, flags=tmp_flags, options=options)
            print(" ".join([str(x) for x in cmd]))
            result = subprocess.run(cmd, capture_output=True, text=True)
            print("Done")

            match = re.search(r"Rendering took\s+(\d+)\s+ms\s*/\s*(\d+)\s+us", result.stdout)
            if match:
                times_ms.append(match.group(1))
                times_us.append(match.group(2))
            else:
                times_ms.append("err")
                times_us.append("err")

            f.write(f"{config['name'].upper()}:\n")
            f.write(result.stdout)

            if result.stderr:
                f.write(f"\nstderr:\n")
                f.write(result.stderr)

            f.write('\n\n')
            f.flush()

    results_file = Path(out_dir) / 'results.txt'
    with open(results_file, 'w') as f:
        f.write('# benchmark frame count on the first line, dataset names on the second line, rendering times in milliseconds on the '
        'third line, and rendering times in microseconds on the fourth line\n')
        f.write(f"{frame_count}\n")
        f.write(','.join([x['name'] for x in configs]) + '\n')
        f.write(','.join(times_ms) + '\n')
        f.write(','.join(times_us) + '\n')


if __name__ == '__main__':
    benchmark_frame_count = str(1000)

    # Barycentric traversal, vertex samples, no macrocell
    run_benchmark(
        '../build/tet_amr_volume_render_b',
        'tet_amr_barycentric_raymarcher',
        'raymarcher',
        benchmark_frame_count
    )

    run_benchmark(
        '../build/tet_amr_volume_render_b',
        'tet_amr_barycentric_woodcock',
        'woodcock',
        benchmark_frame_count
    )

    # Point plane traversal, vertex samples, no macrocell
    run_benchmark(
        '../build/tet_amr_volume_render_p',
        'tet_amr_point_plane_raymarcher',
        'raymarcher',
        benchmark_frame_count
    )

    run_benchmark(
        '../build/tet_amr_volume_render_p',
        'tet_amr_point_plane_woodcock',
        'woodcock',
        benchmark_frame_count
    )

    # Barycentric traversal, vertex samples, with macrocell enabled
    run_benchmark(
        '../build/tet_amr_volume_render_b_m',
        'tet_amr_barycentric_macrocell_raymarcher',
        'raymarcher',
        benchmark_frame_count
    )

    run_benchmark(
        '../build/tet_amr_volume_render_b_m',
        'tet_amr_barycentric_macrocell_woodcock',
        'woodcock',
        benchmark_frame_count
    )

    # Unstructured renderer
    run_benchmark(
        '../build/tet_amr_unstructured_volume_render',
        'tet_amr_unstructured_raymarcher',
        'raymarcher',
        benchmark_frame_count,
        unstructured = True,
    )

    run_benchmark(
        '../build/tet_amr_unstructured_volume_render',
        'tet_amr_unstructured_woodcock',
        'woodcock',
        benchmark_frame_count,
        unstructured = True,
    )

    # Barycentric traversal, cell samples, no macrocell
    cell_benchmark_flags = [
        '--offline',
        '--benchmark',
        '--warmup',
    ]

    run_benchmark(
        '../build/tet_amr_volume_render_b',
        'tet_amr_barycentric_raymarcher_cell',
        'raymarcher',
        benchmark_frame_count,
        flags = cell_benchmark_flags
    )

    run_benchmark(
        '../build/tet_amr_volume_render_b',
        'tet_amr_barycentric_woodcock_cell',
        'woodcock',
        benchmark_frame_count,
        flags = cell_benchmark_flags
    )
