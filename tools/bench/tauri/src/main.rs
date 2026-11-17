use std::io::Write;

#[tauri::command]
fn mode() -> String {
    if std::env::args().any(|a| a == "bench") { "bench".into() } else { "hello".into() }
}

#[tauri::command]
fn ready() {
    println!("READY");
    let _ = std::io::stdout().flush();
}

#[tauri::command]
fn report(app: tauri::AppHandle, line: String) {
    if line == "DONE" {
        app.exit(0);
        return;
    }
    println!("{}", line);
    let _ = std::io::stdout().flush();
}

fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![mode, ready, report])
        .run(tauri::generate_context!())
        .expect("run");
}
