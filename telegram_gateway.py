import requests
import os
import sys
from datetime import datetime
from dotenv import load_dotenv

# Load environment variables
load_dotenv()

def send_to_telegram(audio_path, duration):
    """
    Broadcasts the audio file to multiple Telegram IDs from .env.
    """
    token = os.getenv("TELEGRAM_TOKEN")
    chat_id_raw = os.getenv("CHAT_ID")

    if not token or not chat_id_raw:
        print("[ERROR] TELEGRAM_TOKEN or CHAT_ID not defined in .env")
        sys.exit(1)

    # Flexible ID Parsing: comma-separated, split and strip
    destinations = [cid.strip() for cid in chat_id_raw.split(",") if cid.strip()]
    count = len(destinations)
    
    print(f"[BROADCAST] Starting transmission to {count} destinations...")

    url = f"https://api.telegram.org/bot{token}/sendAudio"
    timestamp = datetime.now().strftime("%m/%d/%Y %H:%M:%S")

    caption = (
        "⚠️ Intercepted Transmission!\n\n"
        "📻 Source: Analog Radio (VHF/UHF)\n"
        f"⏱ Duration: {duration:.2f} seconds\n"
        f"🕒 Time: {timestamp}\n\n"
        "Gateway operating via Python."
    )

    for chat_id in destinations:
        try:
            with open(audio_path, 'rb') as audio_file:
                files = {'audio': audio_file}
                payload = {
                    'chat_id': chat_id,
                    'caption': caption,
                    'parse_mode': 'Markdown'
                }
                response = requests.post(url, data=payload, files=files, timeout=30)
                
                if response.status_code == 200:
                    print(f"[OK] Sent to {chat_id}")
                else:
                    error_msg = response.json().get('description', response.text)
                    print(f"[ERROR] Failed to send to {chat_id}: {error_msg}")

        except Exception as e:
            print(f"[ERROR] Failed to send to {chat_id}: {str(e)}")

    # Cleanup Protocol: remove file only after all attempts
    if os.path.exists(audio_path):
        os.remove(audio_path)
    
    print("[FINISHED] Broadcast complete. Local file removed.")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("[!] Usage: python3 telegram_gateway.py <wav_path> <duration>")
        sys.exit(1)

    file_path = sys.argv[1]
    try:
        duration_arg = float(sys.argv[2])
    except ValueError:
        duration_arg = 0.0

    if os.path.exists(file_path):
        send_to_telegram(file_path, duration_arg)
    else:
        print(f"[ERROR] File {file_path} not found.")
        sys.exit(1)
