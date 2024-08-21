import SwiftUI

struct ContentView: View {
    
    let myArduinoIPAddress = "192.168.0.238";
    
    @State private var month = 1
    @State private var day = 1
    @State private var year = 2024
    @State private var hour = 0
    @State private var minute = 0
    @State private var second = 0
    @State private var numberOfZones = 1
    @State private var duration = 0
    @State private var schedule: [SprinklerSchedule] = []
    @State private var params: [String: Any] = [:]
    @State private var responseMessage: String = "Waiting for response..."
    @State private var isSprinklerOn: Bool = false // State to track if the sprinkler is on

    var body: some View {
        
        HStack {
            HStack {
                Text(responseMessage)
                    .padding()
                    .multilineTextAlignment(.center)
            }
        }
        VStack {
            Text("Hello, Monica & Bill!")
            
            // Sprinkler ON LED
            HStack {
                Circle()
                    .fill(isSprinklerOn ? Color.green : Color.gray)
                    .frame(width: 20, height: 20)
                Text("Sprinkler ON")
                    .foregroundColor(isSprinklerOn ? Color.green : Color.gray)
            }
            .padding()

            HStack {
                Button("ON") {
                    sendCommand(command: "ONN", httpMethod: "GET", params: [:])
                    // Send the "HI!" command when ContentView appears
                    sendCommand(command: "HI!", httpMethod: "GET", params: [:])
                }
                .padding()
                Button("OFF") {
                    sendCommand(command: "OFF", httpMethod: "GET", params: [:])
                    // Send the "HI!" command when ContentView appears
                    sendCommand(command: "HI!", httpMethod: "GET", params: [:])
                }
                .padding()
            }

            DatePicker("Set Timestamp", selection: Binding(
                get: { getDate() },
                set: { setDate(date: $0) }
            ), displayedComponents: [.date, .hourAndMinute])
                .padding()

            Button("SET_TIMESTAMP") {
                sendSetTimestampCommand()
            }
            .padding()

            Stepper("Number of Zones: \(numberOfZones)", value: $numberOfZones, in: 1...10)
                .padding()

            Stepper("Duration per Zone: \(duration) min", value: $duration, in: 0...60)
                .padding()

            List {
                ForEach(schedule) { entry in
                    HStack {
                        Picker("Day of the Week", selection: Binding(
                            get: { entry.dayOfWeek },
                            set: { newValue in
                                if let index = schedule.firstIndex(where: { $0.id == entry.id }) {
                                    schedule[index].dayOfWeek = newValue
                                }
                            }
                        )) {
                            ForEach(0..<7) { day in
                                Text(dayName(for: day)).tag(day)
                            }
                        }
                        DatePicker("Time", selection: Binding(
                            get: { entry.time },
                            set: { newValue in
                                if let index = schedule.firstIndex(where: { $0.id == entry.id }) {
                                    schedule[index].time = newValue
                                }
                            }
                        ), displayedComponents: [.hourAndMinute])
                    }
                }
                .onDelete { indexSet in
                    schedule.remove(atOffsets: indexSet)
                }
            }

            Button("Add Schedule Entry") {
                schedule.append(SprinklerSchedule(dayOfWeek: 0, time: Date()))
            }
            .padding()

            Button("SET_SCHEDULE") {
                sendSetScheduleCommand()
            }
            .padding()
        }
        .padding()
        .onAppear {
            // Send the "HI!" command when ContentView appears
            sendCommand(command: "HI!", httpMethod: "GET", params: [:])
        }
    }

    private func sendCommand(command: String, httpMethod: String, params: [String: Any]) {
        print("sendCommand->command: \(command); httpMethod: \(httpMethod)")
        guard let url = URL(string: "http://\(myArduinoIPAddress)/\(command)") else {
            print("Invalid URL")
            return
        }
        var request = URLRequest(url: url)
        request.httpMethod = httpMethod
        if (httpMethod == "POST") {
            request.httpBody = try? JSONSerialization.data(withJSONObject: params)
        }
        let task = URLSession.shared.dataTask(with: request) { data, response, error in
            if let error = error {
                print("Error occurred: \(error)")
                return
            }
            
            if let httpResponse = response as? HTTPURLResponse {
                print("HTTP Status Code: \(httpResponse.statusCode)")
                print("HTTP Method: \(request.httpMethod ?? "Unknown")")
                
                if let mimeType = httpResponse.mimeType {
                    print("MIME Type: \(mimeType)")
                }
            }
            
            if let data = data, let responseString = String(data: data, encoding: .utf8) {
                print("Response Data: \(responseString)")
                handleResponse(responseString)
            } else {
                print("No response data received or unable to decode data.")
            }
        }

        task.resume()
    }

    private func handleResponse(_ responseString: String) {
        // Parse the JSON response
        if let data = responseString.data(using: .utf8),
           let jsonResponse = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
           let status = jsonResponse["status"] as? String {
            let year = String(status.prefix(4))
            let lastCharacter = status.last
            
            // Update the Sprinkler ON LED based on the last character
            DispatchQueue.main.async {
                isSprinklerOn = (lastCharacter == "1")
            }
            
            if year == "1970" {
                // If the year is "1970", call the functions to send the current date and time
                sendDateTimeToArduino { response in
                    DispatchQueue.main.async {
                        responseMessage = response
                    }
                }
            } else {
                DispatchQueue.main.async {
                    responseMessage = "Received valid response: \(status)"
                }
            }
        } else {
            DispatchQueue.main.async {
                responseMessage = "Invalid JSON response"
            }
        }
    }

    private func sendSetTimestampCommand() {
        guard let url = URL(string: "http://\(myArduinoIPAddress)/SET_TIMESTAMP") else { return }
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let parameters: [String: Any] = [
            "month": month,
            "day": day,
            "year": year,
            "hour": hour,
            "minute": minute,
            "second": second
        ]
        request.httpBody = try? JSONSerialization.data(withJSONObject: parameters)
        URLSession.shared.dataTask(with: request).resume()
    }

    private func sendSetScheduleCommand() {
        guard let url = URL(string: "http://\(myArduinoIPAddress)/SET_SCHEDULE") else { return }
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        let scheduleArray = schedule.map { ["dayOfWeek": $0.dayOfWeek, "time": $0.time.timeIntervalSince1970] }
        let parameters: [String: Any] = [
            "numberOfZones": numberOfZones,
            "duration": duration,
            "schedule": scheduleArray
        ]
        request.httpBody = try? JSONSerialization.data(withJSONObject: parameters)
        URLSession.shared.dataTask(with: request).resume()
    }

    private func getDate() -> Date {
        var dateComponents = DateComponents()
        dateComponents.year = year
        dateComponents.month = month
        dateComponents.day = day
        dateComponents.hour = hour
        dateComponents.minute = minute
        dateComponents.second = second
        return Calendar.current.date(from: dateComponents) ?? Date()
    }

    private func setDate(date: Date) {
        let components = Calendar.current.dateComponents([.year, .month, .day, .hour, .minute, .second], from: date)
        year = components.year ?? 2024
        month = components.month ?? 1
        day = components.day ?? 1
        hour = components.hour ?? 0
        minute = components.minute ?? 0
        second = components.second ?? 0
    }

    private func dayName(for index: Int) -> String {
        let dateFormatter = DateFormatter()
        return dateFormatter.weekdaySymbols[index]
    }
    
    private func getCurrentDateTimeISO8601() -> String {
        let formatter = ISO8601DateFormatter()
        formatter.timeZone = TimeZone(identifier: "America/New_York")
        formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        let dateTimeString = formatter.string(from: Date())
        return dateTimeString
    }

    private func sendDateTimeToArduino(completion: @escaping (String) -> Void) {
        let dateTimeString = getCurrentDateTimeISO8601()
        let json: [String: Any] = ["datetime": dateTimeString]
        
        guard let jsonData = try? JSONSerialization.data(withJSONObject: json) else {
            completion("Error: Unable to serialize JSON")
            return
        }
        
        let urlString = "http://\(myArduinoIPAddress)/TIM"
        var request = URLRequest(url: URL(string: urlString)!)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = jsonData

        let task = URLSession.shared.dataTask(with: request) { data, response, error in
            guard let data = data, error == nil else {
                completion("Error: \(error?.localizedDescription ?? "Unknown error")")
                return
            }

            if let httpStatus = response as? HTTPURLResponse, httpStatus.statusCode != 200 {
                completion("HTTP Status Code: \(httpStatus.statusCode)")
                return
            }

            let responseString = String(data: data, encoding: .utf8) ?? "No response data"
            completion(responseString)
        }

        task.resume()
    }
}

struct SprinklerSchedule: Identifiable {
    var id = UUID()
    var dayOfWeek: Int
    var time: Date
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
