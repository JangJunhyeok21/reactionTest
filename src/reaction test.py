import csv
from datetime import datetime
import os
import time

try:
    import serial
except ImportError:
    # Fallback when pyserial is not installed. Provide minimal stubs so the
    # rest of the script can run and produce a clear error message.
    class SerialException(Exception):
        pass

    class _SerialStub:

        def __init__(self, *args, **kwargs):
            raise SerialException(
                "pyserial 모듈이 설치되어 있지 않습니다. 'pip install pyserial'로 설치하세요."
            )

    serial = type(
        "serial", (), {"SerialException": SerialException, "Serial": _SerialStub}
    )

# --- 설정 및 포트 초기화 ---
SERIAL_PORT = "COM5"
BAUD_RATE = 9600


def main():
    # 1. 매 실행시 고유한 파일명 생성
    current_date_str = datetime.now().strftime("%Y%m%d_%H%M%S")
    csv_filename = f"reaction_log_{current_date_str}.csv"

    print(f"[*] 시리얼 모니터링을 시작합니다. 저장 파일: {csv_filename}")
    print("[*] 종료하려면 Ctrl+C를 누르세요.\n")

    # CSV 파일 초기 생성 및 헤더 수정 (순서 변경: 타임스탬프, 감각, 시간)
    with open(csv_filename, mode="w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["Timestamp", "Sense_Type", "Reaction_Time_ms"])

    try:
        # 시리얼 포트 열기
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)  # 아두이노 리셋 후 안정화 대기

        while True:
            if ser.in_waiting > 0:
                # 라인 단위로 읽어오기 및 디코딩 (공백 제거)
                line = ser.readline().decode("utf-8").strip()

                if not line:
                    continue

                # 논리 보완: 아두이노의 시스템 대기 문자열은 경고를 띄우지 않고 자연스럽게 넘김
                if "Get Ready" in line:
                    continue

                try:
                    # 2. 데이터 파싱 (쉼표 기준 분할)
                    sense_type, duration_ms = line.split(",")

                    # 데이터 정제 및 타임스탬프 생성
                    sense_type = sense_type.strip()
                    duration_ms = int(duration_ms.strip())
                    now_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[
                        :-3
                    ]  # ms까지 표기

                    # 3. CSV 파일에 순서 맞춰서 실시간 추가 기록
                    with open(
                        csv_filename, mode="a", newline="", encoding="utf-8"
                    ) as f:
                        writer = csv.writer(f)
                        # 순서 변경 적용: 타임스탬프 -> 감각 -> 소요 시간
                        writer.writerow([now_time, sense_type, duration_ms])

                    print(
                        f"[기록 완료] 시각: {now_time} | 감각: {sense_type} | 속도: {duration_ms}ms"
                    )

                except ValueError:
                    # 데이터 포맷이 완전히 비정상적인 경우에만 경고 출력
                    print(f"[경고] 파싱할 수 없는 데이터 수신: {line}")

    except serial.SerialException as e:
        print(f"[오류] 시리얼 포트 연결 실패 또는 해제됨: {e}")
    except KeyboardInterrupt:
        print("\n[*] 사용자에 의해 프로그램이 종료되었습니다.")


if __name__ == "__main__":
    main()