use crate::protocol::Spoke;

pub const PPI_SIZE: usize = 640;
pub const PPI_NUM_RANGE_BINS: usize = 868;
pub const PPI_ANGLE_TICKS: f64 = 8192.0;

pub fn render_ppi(spokes: &[Spoke]) -> Vec<u8> {
    let size = PPI_SIZE;
    let center = size as f64 / 2.0;
    let max_r = center - 2.0;

    // Black background, full alpha
    let mut pixels = vec![0u8; size * size * 4];
    for y in 0..size {
        for x in 0..size {
            let idx = (y * size + x) * 4;
            pixels[idx + 3] = 255;
        }
    }

    // Draw each spoke as a radial line from center outward.
    // Each spoke covers a thin angular wedge — we draw 1-pixel-wide radial lines,
    // which naturally leaves gaps at the periphery (spiky edges).
    for spoke in spokes {
        let angle_rad = spoke.angle_ticks as f64 / PPI_ANGLE_TICKS * std::f64::consts::TAU;
        // Convert from compass bearing (0=north, CW) to math angle (0=east, CCW)
        let math_angle = std::f64::consts::FRAC_PI_2 - angle_rad;
        let cos_a = math_angle.cos();
        let sin_a = math_angle.sin();

        let num_bins = spoke.echo.len().min(PPI_NUM_RANGE_BINS);
        if num_bins == 0 { continue; }

        // Walk along the radial line, one pixel step at a time
        let steps = max_r as usize;
        for step in 0..steps {
            let r = step as f64 + 1.0;
            let px = (center + r * cos_a) as usize;
            let py = (center - r * sin_a) as usize;
            if px >= size || py >= size { continue; }

            let bin = (r / max_r * num_bins as f64) as usize;
            if bin >= num_bins { continue; }

            let amplitude = spoke.echo[bin];
            if amplitude == 0 { continue; }

            let intensity = (amplitude as u32 * 255 / 63).min(255) as u8;
            let idx = (py * size + px) * 4;
            pixels[idx] = intensity;
            pixels[idx + 1] = (intensity as u32 * 180 / 255) as u8;
            pixels[idx + 2] = (intensity as u32 * 20 / 255) as u8;
        }
    }

    // Range rings and crosshairs (dark gray)
    let c = center as usize;
    for i in 0..size {
        let idx = (c * size + i) * 4;
        pixels[idx] = 54; pixels[idx + 1] = 54; pixels[idx + 2] = 54;
        let idx = (i * size + c) * 4;
        pixels[idx] = 54; pixels[idx + 1] = 54; pixels[idx + 2] = 54;
    }

    for ring in 1..=4 {
        let ring_r = max_r * ring as f64 / 4.0;
        let steps = (ring_r * std::f64::consts::TAU) as usize;
        for s in 0..steps {
            let angle = s as f64 / steps as f64 * std::f64::consts::TAU;
            let px = (center + ring_r * angle.cos()) as usize;
            let py = (center - ring_r * angle.sin()) as usize;
            if px < size && py < size {
                let idx = (py * size + px) * 4;
                pixels[idx] = 54; pixels[idx + 1] = 54; pixels[idx + 2] = 54;
            }
        }
    }

    pixels
}

pub fn encode_png_bytes(pixels: &[u8], width: usize, height: usize) -> Vec<u8> {
    let mut buf = Vec::new();
    {
        let mut encoder = png::Encoder::new(&mut buf, width as u32, height as u32);
        encoder.set_color(png::ColorType::Rgba);
        encoder.set_depth(png::BitDepth::Eight);
        let mut writer = encoder.write_header().expect("write PNG header");
        writer.write_image_data(pixels).expect("write PNG data");
    }
    buf
}

pub fn write_png(path: &str, pixels: &[u8], width: usize, height: usize) {
    let file = std::fs::File::create(path).expect("create PNG file");
    let ref mut w = std::io::BufWriter::new(file);
    let mut encoder = png::Encoder::new(w, width as u32, height as u32);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    let mut writer = encoder.write_header().expect("write PNG header");
    writer.write_image_data(pixels).expect("write PNG data");
}
