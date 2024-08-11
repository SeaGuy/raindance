//
//  ViewController.swift
//  SprinklerControlApp
//
//  Created by William E. Laing, Jr. on 8/6/24.
//

import UIKit

class ViewController: UIViewController {
    
    // IP address of your Arduino
    let arduinoIP = "http://192.168.0.238" // Replace with your Arduino's IP address

    override func viewDidLoad() {
        super.viewDidLoad()
        // Do any additional setup after loading the view.
    }

    @IBAction func sprinklerOnButtonPressed(_ sender: Any) {
        sendHTTPRequest(endpoint: "/ONN")
    }
    
    @IBAction func sprinklerOffButtonPressed(_ sender: Any) {
        sendHTTPRequest(endpoint: "/OFF")
    }
    
    @IBAction func sprinklerDISCONNECTButtonPressed(_ sender: Any) {
        sendHTTPRequest(endpoint: "/DIS")
    }
    
    // Example: An outlet for a label (if you have one)
    @IBOutlet weak var statusLabel: UILabel!
    
    // Function to send HTTP requests to the Arduino
        func sendHTTPRequest(endpoint: String) {
            guard let url = URL(string: arduinoIP + endpoint) else { return }
            let task = URLSession.shared.dataTask(with: url) { data, response, error in
                if let error = error {
                    print("Error: \(error)")
                    return
                }
                if let data = data, let responseString = String(data: data, encoding: .utf8) {
                    print("Response: \(responseString)")
                    DispatchQueue.main.async {
                        self.statusLabel.text = responseString // Update UI if needed
                    }
                }
            }
            task.resume()
        }
    }
