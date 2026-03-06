use crate::protocol::Spoke;

pub const PPI_SIZE: usize = 800;
pub const PPI_NUM_RANGE_BINS: usize = 868;
pub const PPI_ANGLE_TICKS: f64 = 8192.0;

pub fn render_ppi(spokes: &[Spoke]) -> Vec<u8> {
    let size = PPI_SIZE;
    let center = size as f64 / 2.0;
    let max_r = center - 2.0;

    let mut pixels = vec![0u8; size * size * 4];

    for y in 0..size {
        for x in 0..size {
            let idx = (y * size + x) * 4;
            pixels[idx + 3] = 255;
        }
    }

    let mut sorted: Vec<(f64, &[u8])> = spokes.iter()
        .map(|s| (s.angle_ticks as f64 / PPI_ANGLE_TICKS * std::f64::consts::TAU, s.echo.as_slice()))
        .collect();
    sorted.sort_by(|a, b| a.0.partial_cmp(&b.0).unwrap());

    for y in 0..size {
        for x in 0..size {
            let dx = x as f64 - center;
            let dy = -(y as f64 - center);
            let r = (dx * dx + dy * dy).sqrt();
            if r < 1.0 || r > max_r { continue; }

            let mut angle = dy.atan2(dx);
            angle = std::f64::consts::FRAC_PI_2 - angle;
            if angle < 0.0 { angle += std::f64::consts::TAU; }

            let spoke_idx = match sorted.binary_search_by(|s| s.0.partial_cmp(&angle).unwrap()) {
                Ok(i) => i,
                Err(i) => {
                    if i == 0 { sorted.len() - 1 }
                    else if i >= sorted.len() { 0 }
                    else {
                        let d1 = (sorted[i - 1].0 - angle).abs();
                        let d2 = (sorted[i].0 - angle).abs();
                        if d1 < d2 { i - 1 } else { i }
                    }
                }
            };

            let echo = sorted[spoke_idx].1;
            let num_bins = echo.len().min(PPI_NUM_RANGE_BINS);
            if num_bins == 0 { continue; }

            let bin = ((r / max_r) * num_bins as f64) as usize;
            if bin >= num_bins { continue; }

            let amplitude = echo[bin];
            if amplitude == 0 { continue; }

            let intensity = (amplitude as u32 * 255 / 63).min(255) as u8;
            let idx = (y * size + x) * 4;
            pixels[idx] = intensity;
            pixels[idx + 1] = (intensity as u32 * 180 / 255) as u8;
            pixels[idx + 2] = (intensity as u32 * 20 / 255) as u8;
        }
    }

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
