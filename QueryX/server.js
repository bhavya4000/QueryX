const express = require('express');
const axios = require('axios');
const cors = require('cors');

const app = express();
const PORT = 3000;

app.use(cors());

app.get('/search', async (req, res) => {
    const query = req.query.q;
    const apiKey = 'AIzaSyDW1l3-eoeP8gU_ASQmesCNhrl_-sSXTiM';
    const cx = '51892f09822874a2c';

    const maxResults = 100;
    const resultsPerPage = 10;
    let allResults = [];

    try {
        for (let start = 1; start <= maxResults; start += resultsPerPage) {
            const url = `https://www.googleapis.com/customsearch/v1?key=${apiKey}&cx=${cx}&q=${query}&start=${start}&num=${resultsPerPage}`;

            const response = await axios.get(url);
            const items = response.data.items;

            if (items && items.length > 0) {
                allResults.push(...items);
            } else {
                break; // No more results
            }
        }

        res.json({ items: allResults });
    } catch (error) {
        console.error(error);
        res.status(500).send('Error fetching data from Google API');
    }
});

app.listen(PORT, () => {
    console.log(`Server is running on http://localhost:${PORT}`);
});
