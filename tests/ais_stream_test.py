import asyncio, json
import websockets

# The websocket stream to get the data in real time?
async def stream(key: str):
    async with websockets.connect(
        "wss://stream.aisstream.io/v0/stream",
        compression="deflate",
    ) as ws:
        await ws.send(json.dumps({
            "APIKey": key,
            "BoundingBoxes": [[[23.4, 102.1], [5.5, 115.5]]],
            # "FilterMessageTypes": ["PositionReport"]
        }))
        async for payload in ws:
            print(json.loads(payload))

api_key = str(input("Key: "))
asyncio.run(stream(api_key))