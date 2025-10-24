import requests
import time
import random
import datetime
import json

# --- 1. Configuration ---

# Your Firebase Database URL
DATABASE_URL = "https://plant-6e094-default-rtdb.firebaseio.com"

# Your Database Secret (from Firebase Project Settings > Service accounts > Database secrets)
DATABASE_SECRET = "he5qGAK4IQJbm7sRj3Lr9Xn88o6BeQd8VY1bmMpO" # <-- !!! PASTE YOUR SECRET HERE !!!

LOG_INTERVAL_SECONDS = 5  # Log new data every 5 seconds
MAX_HISTORY_POINTS = 50     # Match the ESP32 code

# --- 2. Helper Function to Get Timestamp ---

def get_timestamp_ms():
    """Returns the current time in milliseconds (UTC)."""
    return int(datetime.datetime.now(datetime.timezone.utc).timestamp() * 1000)

# --- 3. Helper Function to Update History (via REST API) ---

def update_history(sensor_name, value, timestamp):
    """
    Fetches the history array, adds a new point, trims it,
    and uploads it back to Firebase using the REST API.
    """
    
    # Define the full URL for the history node
    history_url = f"{DATABASE_URL}/history/{sensor_name}.json"
    
    # Add the database secret as an auth parameter
    auth_params = {'auth': DATABASE_SECRET}
    
    try:
        # 1. GET current history
        # We add a 'shallow=true' to quickly check if data exists, but for arrays, 
        # it's better to just get the whole thing.
        response = requests.get(history_url, params=auth_params)
        response.raise_for_status() # Raise an error for bad responses (4xx, 5xx)

        history_list = response.json()
        
        if history_list is None:
            history_list = []
        
        # 2. Add new data point [timestamp, value]
        new_point = [timestamp, value]
        history_list.append(new_point)
        
        # 3. Trim the list if it's too long
        while len(history_list) > MAX_HISTORY_POINTS:
            history_list.pop(0) # Remove the oldest item (from the front)
            
        # 4. PUT the new list back to Firebase
        # We send the data as a JSON payload
        response = requests.put(history_url, params=auth_params, json=history_list)
        response.raise_for_status()
        
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"\n[Error] Updating history for {sensor_name}: {e}")
        return False


# --- 4. Main Simulation Loop ---

print(f"Starting REST API simulation. Logging data every {LOG_INTERVAL_SECONDS} seconds...")
print(f"Target URL: {DATABASE_URL}")
print("Press Ctrl+C to stop.")

if DATABASE_SECRET == "YOUR_DATABASE_SECRET":
    print("\n" + "="*50)
    print("  ERROR: Please paste your DATABASE_SECRET into the script.")
    print("="*50)
    exit()

while True:
    try:
        # --- A. Generate simulated data ---
        sim_temp = round(random.uniform(65.0, 85.0), 1)
        sim_humidity = round(random.uniform(30.0, 70.0), 1)
        sim_moisture = round(random.uniform(10.0, 80.0), 1)
        current_timestamp = get_timestamp_ms()

        # --- B. Update /current path (using PUT) ---
        
        current_data = {
            'temp': sim_temp,
            'humidity': sim_humidity,
            'moisture': sim_moisture
        }
        
        current_url = f"{DATABASE_URL}/current.json"
        auth_params = {'auth': DATABASE_SECRET}
        
        try:
            # Use requests.put to overwrite data at the /current path
            resp = requests.put(current_url, params=auth_params, json=current_data)
            resp.raise_for_status() # Check for errors
            
            print(f"\n--- Logged at {datetime.datetime.now()} ---")
            print(f"  Current Data: {current_data}")

            # --- C. Update /history paths ---
            print("  Updating history...")
            update_history('temp', sim_temp, current_timestamp)
            update_history('humidity', sim_humidity, current_timestamp)
            update_history('moisture', sim_moisture, current_timestamp)
            print("  History update complete.")

        except requests.exceptions.RequestException as e:
            print(f"\n[Error] Failed to log to Firebase: {e}")
            if e.response:
                print(f"  Response: {e.response.text}")

        
        # Wait for the next interval
        time.sleep(LOG_INTERVAL_SECONDS)

    except KeyboardInterrupt:
        print("\nSimulation stopped by user.")
        break
    except Exception as e:
        print(f"\nAn unknown error occurred: {e}")
        print("Restarting loop in 10 seconds...")
        time.sleep(10)