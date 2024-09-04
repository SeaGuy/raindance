import SwiftUI

struct ContentView: View {
    
    let myArduinoIPAddress = "192.168.0.238";
    
    @State private var month = 1
    @State private var day = 1
    @State private var year = 2024
    @State private var hour = 0
    @State private var minute = 0
    @State private var second = 0
    @State private var numberOfZones = 3
    @State private var duration = 30
    @State private var schedule: [SprinklerSchedule] = []
    @State private var params: [String: Any] = [:]
    @State private var responseMessage: String = "Waiting for response..."
    @State private var isSprinklerOn: Bool = false // State to track if the sprinkler is on
    @State private var blinkOpacity: Double = 1.0 // for the blinking effect on "Sprinkler is ON"
    @State private var showAlert = false  // to limit number of schedule entries

    
    var body: some View {
        
        VStack {
            Text("Hello, Monica & Bill!")
            
            // Sprinkler ON LED
            HStack {
                Circle()
                    .fill(isSprinklerOn ? Color.green : Color.gray)
                    .frame(width: 20, height: 20)
                Text(isSprinklerOn ? "Sprinkler is ON" : "Sprinkler is OFF")
                    .foregroundColor(isSprinklerOn ? Color.green : Color.gray)
                    .opacity(isSprinklerOn && blinkOpacity == 0.0 ? 0.0 : 1.0) // Control opacity based on blinking state
                    .animation(isSprinklerOn ? .easeInOut(duration: 0.5).repeatForever(autoreverses: true) : .none, value: blinkOpacity)  // Start/stop animation based on sprinkler state
                
            }
            .padding()
            
            HStack {
                Button("ON") {
                    sendCommand(command: "ONN", httpMethod: "GET", params: [:])
                    sendCommand(command: "HI!", httpMethod: "GET", params: [:])
                }
                .padding()
                .background(Color.green)
                .foregroundColor(.white)
                .cornerRadius(10)
                .shadow(radius: 5)
                
                Button("OFF") {
                    sendCommand(command: "OFF", httpMethod: "GET", params: [:])
                    sendCommand(command: "HI!", httpMethod: "GET", params: [:])
                }
                .padding()
                .background(Color.gray)
                .foregroundColor(.white)
                .cornerRadius(10)
                .shadow(radius: 5)
            }
            
            // Schedule settings and entries inside a bounding box
            GroupBox(label: Label("Schedule Settings", systemImage: "calendar")) {
                VStack {
                    Stepper("Number of Zones: \(numberOfZones)", value: $numberOfZones, in: 1...10)
                        .padding([.top, .horizontal])

                    Stepper("Duration per Zone: \(duration) min", value: $duration, in: 0...60)
                        .padding([.horizontal, .bottom])

                    Divider() // Divider between steppers and schedule entries

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
                                .pickerStyle(MenuPickerStyle()) // Use a compact style for the Picker
                                .frame(maxWidth: .infinity) // Ensure Picker uses available space
                                
                                DatePicker("Time", selection: Binding(
                                    get: { entry.time },
                                    set: { newValue in
                                        if let index = schedule.firstIndex(where: { $0.id == entry.id }) {
                                            schedule[index].time = newValue
                                        }
                                    }
                                ), displayedComponents: [.hourAndMinute])
                                .labelsHidden() // Hide labels to make it more compact
                                .frame(maxWidth: .infinity) // Ensure DatePicker uses available space
                            }
                        }
                        .onDelete { indexSet in
                            schedule.remove(atOffsets: indexSet)
                        }
                    }
                    .frame(maxHeight: 300) // Set a maximum height for the List to ensure visibility
                }
                //.padding()
            }
            //.padding()

            HStack {
                Button("Add") {
                    if schedule.count < 2 {
                        schedule.append(SprinklerSchedule(dayOfWeek: 0, time: Date()))
                    } else {
                        showAlert = true // Trigger the alert
                    }
                }
                .padding()
                .background(Color.gray)
                .foregroundColor(.white)
                .cornerRadius(10)
                .shadow(radius: 5)
                .alert(isPresented: $showAlert) {
                    Alert(title: Text("Limit Reached"), message: Text("You can only add up to 2 schedules."), dismissButton: .default(Text("OK")))
                }
                
                Button("Upload") {
                    sendSetScheduleCommand()
                }
                .padding()
                .background(Color.gray)
                .foregroundColor(.white)
                .cornerRadius(10)
                .shadow(radius: 5)
            }
            
        }
        .padding()
        
        .onAppear {
            // Send the "HI!" command when ContentView appears
            sendCommand(command: "HI!", httpMethod: "GET", params: [:])
            if isSprinklerOn {
                startBlinking()
            }
        }
        .onChange(of: isSprinklerOn) { newValue in
            if newValue {
                startBlinking()
            } else {
                stopBlinking()
            }
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
    
    /* private func sendSetScheduleCommand() {
        guard let url = URL(string: "http://\(myArduinoIPAddress)/SCH") else { return }
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
    } */
    
    private func sendSetScheduleCommand() {
        guard let url = URL(string: "http://\(myArduinoIPAddress)/SCH") else { return }
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        
        // Convert the time to "HH:mm" format for each schedule entry
        let timeFormatter = DateFormatter()
        timeFormatter.dateFormat = "HH:mm"
        
        let scheduleArray = schedule.map {
            ["dayOfWeek": $0.dayOfWeek, "time": timeFormatter.string(from: $0.time)]
        }
        
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
    
    func performActionWithDelay(delay: Double, action: @escaping () -> Void) {
        DispatchQueue.main.asyncAfter(deadline: .now() + delay) {
            action()  // Execute the passed function after the delay
        }
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
    
    private func startBlinking() {
        blinkOpacity = 0.0
        withAnimation {
            blinkOpacity = 0.0
        }
    }
    
    private func stopBlinking() {
        withAnimation {
            blinkOpacity = 1.0
        }
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
